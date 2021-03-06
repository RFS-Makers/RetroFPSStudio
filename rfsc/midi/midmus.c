// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <midi-parser.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midi/midmus.h"


#define SMF_MAX_CHANNELS 16

typedef struct midmussong_midiinfotrackplayingnote {
    int srcchannel;
    int velocity;
    int pan;
    int key;
    int startmeasure;
    double startmeasurepos;
} midmussong_midiinfotrackplayingnote;

typedef struct midmussong_midiinfotrack {
    int16_t midiinstrument;
    int midisourcetrack;
    int midiorigfirstsourcechannel;

    int playingnotecount, playingnotealloc;
    midmussong_midiinfotrackplayingnote *playingnote;
} midmussong_midiinfotrack;

typedef struct midmussong_midiinfosourcechannel {
    int16_t midiinstrument;
    int32_t pan;
    int32_t modeventcount;
    midmusmodify *modevent;
} midmussong_midiinfosourcechannel;

typedef struct midmussong_midiinfosourcetrack {
    midmussong_midiinfosourcechannel channel[SMF_MAX_CHANNELS];
} midmussong_midiinfosourcetrack;

typedef struct midmussong_midiinfotimesig {
    int32_t midiclock;
    int32_t signom, sigdiv;
} midmussong_midiinfotimesig;

typedef struct midmussong_midiinfo {
    midmussong *song;

    midmussong_midiinfotrack *songtrackinfo;

    int32_t timesigchangecount;
    midmussong_midiinfotimesig *timesigchange;

    int midippq;  // Parts Per Quarter (=clock ticks per beat)
    int sourcetrackcount;
    midmussong_midiinfosourcetrack *sourcetrackinfo;
} midmussong_midiinfo;


static int _midmussong_KeepMidiTimeSigChange(
        midmussong_midiinfo *minfo,
        int32_t clock, int32_t signom, int32_t sigdiv) {
    int32_t newcount = minfo->timesigchangecount + 1;
    midmussong_midiinfotimesig *newtimesigchange = (
        realloc(minfo->timesigchange,
            newcount * sizeof(*minfo->timesigchange)));
    if (!newtimesigchange)
        return 0;
    memset(&newtimesigchange[newcount - 1],
        0, sizeof(*newtimesigchange));
    newtimesigchange[newcount - 1].midiclock = clock;
    newtimesigchange[newcount - 1].signom = signom;
    newtimesigchange[newcount - 1].sigdiv = sigdiv;
    minfo->timesigchange = newtimesigchange;
    minfo->timesigchangecount = newcount;
    return 1;
}


static void midmussong_FreeMidiInfoContents(
        midmussong_midiinfo *minfo) {
    if (!minfo)
        return;
    free(minfo->timesigchange);
    int i = 0;
    while (i < minfo->song->trackcount) {
        free(minfo->songtrackinfo[i].playingnote);
        i++;
    }
    if (minfo->sourcetrackinfo) {
        int i = 0;
        while (i < minfo->sourcetrackcount) {
            int k = 0;
            while (k < 16) {
                free(minfo->sourcetrackinfo[i].channel[k].modevent);
                k++;
            }
            i++;
        }
        free(minfo->sourcetrackinfo);
    }
    free(minfo->songtrackinfo);
    memset(minfo, 0, sizeof(*minfo));
}


void midmussong_UpdateMeasureTiming(
        midmusmeasure *measure) {  // Update measure length based on
                                   // time signature and BPM.
    assert(measure->signaturediv > 0);
    measure->beatspermeasure = (
        (double)measure->signaturenom /
        (double)measure->signaturediv) * 4.0;
    if (fabs(measure->beatspermeasure -
            round(measure->beatspermeasure)) < 0.01 &&
            round(measure->beatspermeasure) > 0) {
        // Avoid rounding issues by erring towards whole numbers.
        measure->beatspermeasure = round(
            measure->beatspermeasure);
    }
    double beatsseconds = 1.0 / (
        (double)measure->bpm / 60.0);
    measure->framelen = round(
        ((double)MIDMUS_SAMPLERATE) * beatsseconds *
        ((double)measure->beatspermeasure));
}


int midmussong_EnsureMeasureCount(
        midmussong *song, int32_t count) {
    if (count >= song->measurecount) {
        midmusmeasure *newmeasure = realloc(
            song->measure, sizeof(*newmeasure) * (
            count));
        if (!newmeasure)
            return 0;
        song->measure = newmeasure;
        memset(&song->measure[song->measurecount], 0,
            sizeof(*newmeasure) *
            (count - song->measurecount));
        // If we got previous measures, keep tempo and signatures:
        int k = song->measurecount;
        while (k < count) {
            if (k == 0) {
                // If first measure, set defaults:
                song->measure[0].signaturenom = 4;
                song->measure[0].signaturediv = 4;
                song->measure[0].bpm = 120;
            } else {
                song->measure[k].signaturenom =
                    song->measure[k - 1].signaturenom;
                song->measure[k].signaturediv =
                    song->measure[k - 1].signaturediv;
                song->measure[k].bpm =
                    song->measure[k - 1].bpm;
            }
            midmussong_UpdateMeasureTiming(
                &song->measure[k]);
            k++;
        }
        // Done, set new count:
        song->measurecount = count;
    }
    return 1;
}


int midmussong_ExpandBlockToPoint(midmusblock *bl,
        int32_t measure, double measurepos) {
    if (bl->measurestart > measure) {
        int32_t expandforward = (bl->measurestart - measure);
        bl->measurestart -= expandforward;
        bl->measurelen += expandforward;
    }
    assert(bl->measurestart <= measure);
    assert(bl->measurestart >= 0);
    if (bl->measurelen < 1)
        bl->measurelen = 1;
    if (measurepos > 0) {
        if (bl->measurestart + bl->measurelen < measure + 1) {
            bl->measurelen = (
                measure - bl->measurestart + 1);
        }
    } else {
        if (bl->measurestart + bl->measurelen < measure) {
            bl->measurelen = (
                measure - bl->measurestart);
        }
    }
    assert(bl->measurestart >= 0);
    assert(bl->measurelen > 0);
    if (!midmussong_EnsureMeasureCount(
            bl->parent->parent,
            bl->measurestart + bl->measurelen + 1)) {
        return 0;
    }
    return 1;
}


static int midmussong_FeedMidiNoteOnToTrack(
        midmussong_midiinfo *minfo, int track,
        ATTR_UNUSED int srctrack,
        int channel, int measure, double measurepos,
        int key, int velocity, int pan) {
    assert(velocity > 0);
    assert(pan >= -MIDMUS_PANRANGE && pan <= MIDMUS_PANRANGE);
    assert(minfo->songtrackinfo[track].midiinstrument ==
        minfo->sourcetrackinfo[srctrack].
            channel[channel].midiinstrument);
    if (minfo->song->track[track].blockcount == 0) {
        minfo->song->track[track].block = malloc(
            sizeof(*minfo->song->track[track].block));
        if (!minfo->song->track[track].block)
            return 0;
        memset(minfo->song->track[track].block, 0,
            sizeof(*minfo->song->track[track].block));
        minfo->song->track[track].block[0].no = 0;
        minfo->song->track[track].block[0].parent =
            &minfo->song->track[track];
        minfo->song->track[track].blockcount = 1;
    }
    if (!midmussong_EnsureMeasureCount(
            minfo->song, measure + 1)) {
        return 0;
    }
    if (!midmussong_ExpandBlockToPoint(
            &minfo->song->track[track].block[0],
            measure, measurepos))
        return 0;
    if (minfo->songtrackinfo[track].playingnotecount + 1 >
            minfo->songtrackinfo[track].playingnotealloc) {
        int newalloc = (
            minfo->songtrackinfo[track].playingnotecount + 1);
        midmussong_midiinfotrackplayingnote *newnote = realloc(
            minfo->songtrackinfo[track].playingnote,
            sizeof(*newnote) * newalloc);
        if (!newnote)
            return 0;
        minfo->songtrackinfo[track].playingnote = newnote;
        memset(&minfo->songtrackinfo[track].playingnote[
            minfo->songtrackinfo[track].playingnotealloc],
            0, sizeof(*newnote) * (
            newalloc - minfo->songtrackinfo[track].playingnotealloc));
        minfo->songtrackinfo[track].playingnotealloc = newalloc;
    }
    minfo->songtrackinfo[track].playingnotecount++;
    memset(&minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1], 0,
        sizeof(*minfo->songtrackinfo[track].playingnote));
    minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1].
            srcchannel = channel;
    minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1].
            startmeasure = measure;
    minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1].
            startmeasurepos = measurepos;
    minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1].
            velocity = velocity;
    minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1].
            pan = pan;
    minfo->songtrackinfo[track].playingnote[
        minfo->songtrackinfo[track].playingnotecount - 1].
            key = key;
    return 1;
}


static int midmussong_FinalizeMidiNoteOnTrack(
        midmussong_midiinfo *minfo, int track,
        ATTR_UNUSED int srctrack,
        int noteidx, int endmeasure, double endmeasurepos) {
    assert(minfo->song->track[track].blockcount > 0);

    // Ensure we got measure meta info up to this point:
    if (!midmussong_EnsureMeasureCount(
            minfo->song, endmeasure + 1)) {
        return 0;
    }
    // Block must contain note:
    if (!midmussong_ExpandBlockToPoint(
            &minfo->song->track[track].block[0],
            endmeasure, endmeasurepos))
        return 0;

    // Allocate new final note entry:
    midmusnote *newnote = realloc(
        minfo->song->track[track].block[0].note,
        sizeof(*newnote) * (
        minfo->song->track[track].block[0].notecount + 1));
    if (!newnote)
        return 0;
    minfo->song->track[track].block[0].note = newnote;

    // Set all the info on our new midi note:
    midmusnote *insertnoteref = &(
        minfo->song->track[track].block[0].note[
        minfo->song->track[track].block[0].notecount]);
    memset(insertnoteref, 0, sizeof(*insertnoteref));
    insertnoteref->munitoffset = (int64_t)roundl(
        (double)MIDMUS_MEASUREUNITS *
        (double)minfo->songtrackinfo[track].
            playingnote[noteidx].startmeasurepos) +
        (int64_t)(MIDMUS_MEASUREUNITS) *
        (int64_t)minfo->songtrackinfo[track].
            playingnote[noteidx].startmeasure;
    int64_t endoffset = roundl(
        (double)MIDMUS_MEASUREUNITS *
        ((double)endmeasure + (double)endmeasurepos));
    insertnoteref->munitlen = (endoffset -
        insertnoteref->munitoffset);
    if (insertnoteref->munitlen < 1)
        insertnoteref->munitlen = 1;
    int vel = minfo->songtrackinfo[track].
        playingnote[noteidx].velocity;
    int key = minfo->songtrackinfo[track].
        playingnote[noteidx].key;
    insertnoteref->volume = (vel <= 127 ?
        (vel >= 0 ? vel : 0) : 127);
    insertnoteref->key = (key <= 127 ?
        (key >= 0 ? key : 0) : 127);
    assert(MIDMUS_PANRANGE * 2 < 127);
    insertnoteref->pan = ((int32_t)127 +
        (int32_t)minfo->songtrackinfo[track].
        playingnote[noteidx].pan);

    // Increase note count now that we're done adding one:
    minfo->song->track[track].block[0].notecount++;

    // Then, remove the old playing note ref since we had note off:
    if (noteidx < minfo->songtrackinfo[track].playingnotecount - 1) {
        memmove(
            &minfo->songtrackinfo[track].playingnote[noteidx],
            &minfo->songtrackinfo[track].playingnote[noteidx + 1],
            sizeof(*minfo->songtrackinfo[track].playingnote) *
            (minfo->songtrackinfo[track].playingnotecount - noteidx - 1));
    }
    minfo->songtrackinfo[track].playingnotecount--;

    // Now, see if we overlap with other notes and how many:
    return 1;
}


static void midmussong_RecalculateBlockNoteOverlaps(
        midmusblock *bl) {
    bl->maxoverlappingnotes = 0;
    int notecount = bl->notecount;
    int k = 0;
    while (k < notecount) {
        int overlapcount = 0;
        int i = 0;
        while (i < notecount && i < k) {
            if (bl->note[i].key !=
                    bl->note[k].key) {
                i++;
                continue;
            }
            if ((bl->note[i].
                        munitoffset >=
                    bl->note[k].munitoffset &&
                    bl->note[i].
                        munitoffset <
                    bl->note[k].munitoffset +
                        bl->note[k].munitlen
                    ) || (
                    bl->note[i].
                        munitoffset + bl->note[i].
                        munitlen >=
                    bl->note[k].munitoffset &&
                    bl->note[i].
                        munitoffset + bl->note[i].
                        munitlen <
                    bl->note[k].munitoffset +
                        bl->note[k].munitlen
                    ) || (
                    bl->note[i].munitoffset <=
                    bl->note[k].munitoffset &&
                    bl->note[i].munitoffset +
                        bl->note[i].munitlen >=
                    bl->note[k].munitoffset +
                        bl->note[k].munitlen
                    )) {
                overlapcount++;
            }
            i++;
        }
        bl->note[k].overlapindex = overlapcount;
        if (overlapcount + 1 > bl->maxoverlappingnotes)
            bl->maxoverlappingnotes = (
                overlapcount + 1);
        if (bl->maxoverlappingnotes >
                bl->parent->maxoverlappingnotes)
            bl->parent->maxoverlappingnotes = (
                bl->maxoverlappingnotes);
        k++;
    }
}


static int midmussong_FeedMidiNoteOffToTrack(
        midmussong_midiinfo *minfo, int track, int srctrack,
        int channel, int key, int measure, double measurepos) {
    if (minfo->song->track[track].blockcount == 0)
        return 1;  // nothing to do with this

    int i = 0;
    while (i < minfo->songtrackinfo[track].playingnotecount) {
        if (minfo->songtrackinfo[track].playingnote[i].
                srcchannel == channel &&
                minfo->songtrackinfo[track].playingnote[i].key == key) {
            return midmussong_FinalizeMidiNoteOnTrack(
                minfo, track, srctrack, i, measure, measurepos);
        }
        i++;
    }

    #if defined(DEBUG_MIDIPARSER)
    printf("rfsc/midi/midmus.c: warning: "
        "ignoring bogus midi note off event\n");
    #endif
    return 1;
}


static int midmussong_FeedMidiAllNotesOffToTrack(
        midmussong_midiinfo *minfo, int track, int srctrack,
        int channel, int measure, double measurepos) {
    if (minfo->song->track[track].blockcount == 0)
        return 1;  // nothing to do with this

    int count = 0;
    int i = 0;
    while (i < minfo->songtrackinfo[track].playingnotecount) {
        if (minfo->songtrackinfo[track].playingnote[i].
                srcchannel == channel) {
            count++;
            if (!midmussong_FinalizeMidiNoteOnTrack(
                    minfo, track, srctrack, i,
                    measure, measurepos))
                return 0;
        }
        i++;
    }
    if (count > 0) {
        #if defined(DEBUG_MIDIPARSER)
        printf("rfsc/midi/midmus.c: debug: "
            "all notes off on src track %d channel %d "
            "stopped %d notes at measure %d, measure pos %f\n",
            srctrack, channel, count, measure, measurepos);
        #endif
    }

    return 1;
}


static int midmussong_EnsureSourceTrackInfo(
        midmussong_midiinfo *minfo, int srctrack) {
    assert(srctrack >= 0);
    if (srctrack >= minfo->sourcetrackcount) {
        midmussong_midiinfosourcetrack *newsourcetinfo = realloc(
            minfo->sourcetrackinfo, sizeof(*newsourcetinfo) * (
            srctrack + 1));
        if (!newsourcetinfo)
            return 0;
        minfo->sourcetrackinfo = newsourcetinfo;
        memset(&minfo->sourcetrackinfo[minfo->sourcetrackcount], 0,
            sizeof(*newsourcetinfo) *
            (srctrack + 1 - minfo->sourcetrackcount));
        minfo->sourcetrackcount = srctrack + 1;
    } else {
        assert(minfo->sourcetrackinfo != NULL);
    }
    return 1;
}


static int midmussong_FeedMidiAllNotesOff(
        midmussong_midiinfo *minfo, int srctrack,
        int channel, int measure, double measurepos
        ) {
    // Ensure allocation of info struct holding info on SOURCE track:
    // (The source is a simple midi file/official midi standard track)
    if (!midmussong_EnsureSourceTrackInfo(minfo, srctrack))
        return 0;

    // Find the TARGET track to assign the event to:
    // (The target is a track in our own midmus song format.)
    int i = 0;
    while (i < minfo->song->trackcount) {
        if (minfo->songtrackinfo[i].midisourcetrack == srctrack) {
            return midmussong_FeedMidiAllNotesOffToTrack(
                minfo, i, srctrack,
                channel, measure, measurepos);
        }
        i++;
    }

    // Since we have no corresponding track, this is a bogus event.
    // Just ignore it.
    return 1;
}


static int midmussong_FeedMidiNoteOff(
        midmussong_midiinfo *minfo, int srctrack,
        int channel, int key, int measure, double measurepos
        ) {
    // Ensure allocation of info struct holding info on SOURCE track:
    // (The source is a simple midi file/official midi standard track)
    if (!midmussong_EnsureSourceTrackInfo(minfo, srctrack))
        return 0;

    // Find the TARGET track to assign the event to:
    // (The target is a track in our own midmus song format.)
    int i = 0;
    while (i < minfo->song->trackcount) {
        if (minfo->songtrackinfo[i].midisourcetrack == srctrack &&
                (minfo->songtrackinfo[i].midiinstrument == -1 ||
                minfo->songtrackinfo[i].midiinstrument ==
                minfo->sourcetrackinfo[srctrack].
                    channel[channel].midiinstrument)) {
            return midmussong_FeedMidiNoteOffToTrack(
                minfo, i, srctrack,
                channel, key, measure, measurepos);
        }
        i++;
    }

    // Since we have no corresponding track, this is a bogus event.
    // Just ignore it.
    return 1;
}


static int _midmussong_CanMergeMidiChannels(
        midmussong_midiinfo *minfo, int srctrack, int chan1, int chan2
        ) {
    if (chan1 == chan2)
        return 1;
    assert(chan1 >= 0 && chan1 < 16 && chan2 >= 0 && chan2 < 16);
    if (minfo->sourcetrackinfo[srctrack].channel[chan1].modeventcount !=
            minfo->sourcetrackinfo[srctrack].channel[chan2].modeventcount)
        return 0;
    int i = 0;
    while (i < minfo->sourcetrackinfo[srctrack].
            channel[chan1].modeventcount) {
        if (minfo->sourcetrackinfo[srctrack].channel[chan1].
                modevent[i].type !=
                minfo->sourcetrackinfo[srctrack].channel[chan2].
                modevent[i].type ||
                minfo->sourcetrackinfo[srctrack].channel[chan1].
                modevent[i].value !=
                minfo->sourcetrackinfo[srctrack].channel[chan2].
                modevent[i].value ||
                minfo->sourcetrackinfo[srctrack].channel[chan1].
                modevent[i].munitoffset !=
                minfo->sourcetrackinfo[srctrack].channel[chan2].
                modevent[i].munitoffset)
            return 0;
        i++;
    }
    return 1;
}


static int midmussong_FeedMidiNoteOn(
        midmussong_midiinfo *minfo, int srctrack,
        int channel, int measure, double measurepos,
        int key, int velocity) {
    assert(srctrack >= 0);
    assert(channel >= 0 && channel < SMF_MAX_CHANNELS);
    if (velocity > 127) velocity = 127;
    if (velocity <= 0 ||
            minfo->sourcetrackinfo[srctrack].
            channel[channel].midiinstrument == -1)
        return midmussong_FeedMidiNoteOff(minfo, srctrack,
            channel, key, measure, measurepos);

    // Ensure allocation of info struct holding info on SOURCE track:
    // (The source is a simple midi file/official midi standard track)
    if (!midmussong_EnsureSourceTrackInfo(minfo, srctrack))
        return 0;

    // Find the TARGET track to assign the event to:
    // (The target is a track in our own midmus song format.)
    int i = 0;
    while (i < minfo->song->trackcount) {
        if (minfo->songtrackinfo[i].midisourcetrack == srctrack &&
                (minfo->songtrackinfo[i].midiinstrument == -1 ||
                minfo->songtrackinfo[i].midiinstrument ==
                minfo->sourcetrackinfo[srctrack].
                    channel[channel].midiinstrument) &&
                _midmussong_CanMergeMidiChannels(
                    minfo, srctrack,
                    minfo->songtrackinfo[i].midiorigfirstsourcechannel,
                    channel
                    )) {
            return midmussong_FeedMidiNoteOnToTrack(
                minfo, i, srctrack,
                channel, measure, measurepos, key, velocity,
                minfo->sourcetrackinfo[srctrack].
                channel[channel].pan);
        }
        i++;
    }

    // If we arrive here, there is no suitable TARGET track yet.
    // Allocate new track:
    midmussong_midiinfotrack *newtinfo = realloc(
        minfo->songtrackinfo, sizeof(*newtinfo) * (
        minfo->song->trackcount + 1));
    if (!newtinfo)
        return 0;
    minfo->songtrackinfo = newtinfo;
    memset(&minfo->songtrackinfo[minfo->song->trackcount], 0,
        sizeof(*newtinfo));
    minfo->songtrackinfo[minfo->song->trackcount].
        midiorigfirstsourcechannel = channel;
    minfo->songtrackinfo[minfo->song->trackcount].
        midiinstrument = minfo->sourcetrackinfo[srctrack].
            channel[channel].midiinstrument;
    minfo->songtrackinfo[i].midisourcetrack = srctrack;
    midmustrack *newtrack = realloc(
        minfo->song->track, sizeof(*newtrack) * (
        minfo->song->trackcount + 1));
    if (!newtrack)
        return 0;
    minfo->song->track = newtrack;
    memset(&minfo->song->track[minfo->song->trackcount], 0,
        sizeof(*newtrack));
    minfo->song->track[minfo->song->trackcount].instrument =
        minfo->songtrackinfo[minfo->song->trackcount].
        midiinstrument;
    minfo->song->track[minfo->song->trackcount].volume = 127;
    minfo->song->track[minfo->song->trackcount].parent =
        minfo->song;
    minfo->song->trackcount++;
    {  // All block parents need updating due to tracks realloc:
        int k = 0;
        while (k < minfo->song->trackcount) {
            int k2 = 0;
            while (k2 < minfo->song->track[k].blockcount) {
                minfo->song->track[k].block[k2].parent =
                    &minfo->song->track[k];
                k2++;
            }
            k++;
        }
    }

    // Add note to newly allocated track:
    return midmussong_FeedMidiNoteOnToTrack(
        minfo, minfo->song->trackcount - 1, srctrack,
        channel, measure, measurepos, key, velocity,
        minfo->sourcetrackinfo[srctrack].channel[channel].
            pan);
}


static int32_t _miditicksofmeasure(
        midmussong_midiinfo *minfo, int measure) {
    assert(measure >= 0 && measure < minfo->song->measurecount);
    assert(minfo->midippq > 0);
    midmussong_UpdateMeasureTiming(&minfo->song->measure[measure]);
    int32_t result = round(
        (double)minfo->midippq *
        minfo->song->measure[measure].beatspermeasure);
    if (result < 1)
        result = 1;
    return result;
}


static int64_t _framestartofmeasure(
        midmussong *song, int measure) {
    assert(measure >= 0 && measure < song->measurecount);
    int64_t offset = 0;
    int k = 0;
    while (k < song->measurecount && k < measure) {
        midmussong_UpdateMeasureTiming(&song->measure[k]);
        offset += song->measure[k].framelen;
        k++;
    }
    return offset;
}


int32_t midmussong_FrameOffsetToMeasure(
        midmussong *song, int64_t frameoffset) {
    if (frameoffset < 0)
        return 0;
    int64_t offset = 0;
    int32_t i = 0;
    while (i < song->measurecount) {
        if (song->measure[i].framelen + offset > frameoffset &&
                offset <= frameoffset)
            return i;
        offset += song->measure[i].framelen;
        i++;
    }
    return song->measurecount;
}


void midmussong_SetMeasureBPM(midmussong *song,
        int measure, double bpm) {
    if (measure >= song->measurecount)
        return;
    double foundbpm = song->measure[measure].bpm;
    int k = measure;
    while (k < song->measurecount &&
            song->measure[k].bpm == foundbpm) {
        song->measure[k].bpm = bpm;
        midmussong_UpdateMeasureTiming(&song->measure[k]);
        k++;
    }
}


void midmussong_SetMeasureTimeSig(
        midmussong *song,
        int measure, int32_t nom, int32_t div) {
    if (measure < 0) measure = 0;
    if (measure >= song->measurecount)
        return;
    int32_t foundnom = song->measure[measure].signaturenom;
    int32_t founddiv = song->measure[measure].signaturediv;
    int k = measure;
    while (k < song->measurecount &&
            song->measure[k].signaturenom == foundnom &&
            song->measure[k].signaturediv == founddiv) {
        song->measure[k].signaturenom = nom;
        song->measure[k].signaturediv = div;
        midmussong_UpdateMeasureTiming(&song->measure[k]);
        k++;
    }
}


static int _miditimetomeasure(midmussong_midiinfo *minfo,
        int32_t clock, int32_t *out_measure,
        double *out_posinmeasure) {
    int64_t tickoffset = 0;
    int k = 0;
    while (1) {
        if (!midmussong_EnsureMeasureCount(minfo->song, k + 1))
            return 0;
        int64_t measurelen = _miditicksofmeasure(minfo, k);
        if (tickoffset <= clock && tickoffset + measurelen > clock) {
            *out_measure = k;
            *out_posinmeasure = fmax(0, fmin(1,
                (double)(clock - tickoffset) / (double)measurelen));
            return 1;
        }
        k++;
        tickoffset += measurelen;
    }
}


uint64_t midmussong_GetFramesLength(midmussong *s) {
    uint64_t longest_track_frames = 0;
    int i = 0;
    while (i < s->trackcount) {
        int k = 0;
        while (k < s->track[i].blockcount) {
            uint64_t pastblockframe = (
                s->track[i].block[k].frameoffset +
                s->track[i].block[k].framelen);
            if (pastblockframe > longest_track_frames)
                longest_track_frames = pastblockframe;
            k++;
        }
        i++;
    }
    return longest_track_frames;
}


double midmussong_GetSecondsLength(midmussong *s) {
    uint64_t frames = midmussong_GetFramesLength(s);
    uint64_t base_seconds = frames / MIDMUS_SAMPLERATE;
    frames = (frames % MIDMUS_SAMPLERATE);
    double result = ((double)base_seconds) +
        ((double)frames) / ((double)MIDMUS_SAMPLERATE);
    return result;
}


void midmussong_GetMeasureStartLen(
        midmussong *song, int measure,
        int64_t *out_framestart, int64_t *out_framelen) {
    if (measure < 0) {
        *out_framestart = 0;
        *out_framelen = 1;
    }
    int64_t startoffset = 0;
    int k = 0;
    while (k < song->measurecount && k < measure) {
        startoffset += song->measure[k].framelen;
        k++;
    }
    *out_framestart = startoffset;
    if (k < song->measurecount) {
        *out_framelen = song->measure[k].framelen;
    } else if (k > 0) {
        *out_framelen = song->measure[k - 1].framelen;
    } else {
        *out_framelen = 1;
    }
}


void midmussong_UpdateBlockSamplePos(
        midmusblock *bl) {
    if (bl->measurelen <= 0)
        bl->measurelen = 1;
    bl->frameoffset = _framestartofmeasure(
        bl->parent->parent, bl->measurestart);
    bl->framelen = _framestartofmeasure(
        bl->parent->parent, bl->measurestart +
        bl->measurelen) - bl->frameoffset;
    int32_t i = 0;
    while (i < bl->notecount) {
        midmusnote *noteref = &(bl->note[i]);
        int64_t note_measurestart = (
            (int32_t)noteref->munitoffset
            / (int32_t)MIDMUS_MEASUREUNITS) + bl->measurestart;
        int64_t note_measureoffset = (
            (int64_t)noteref->munitoffset - (
            (int64_t)MIDMUS_MEASUREUNITS *
            (note_measurestart - (int64_t)bl->measurestart)));
        noteref->frameoffsetinblock = (
            _framestartofmeasure(bl->parent->parent,
            note_measurestart)) - _framestartofmeasure(
            bl->parent->parent, bl->measurestart) +
            ((int64_t)
            bl->parent->parent->measure[note_measurestart].
            framelen * (int64_t)note_measureoffset /
            (int64_t)MIDMUS_MEASUREUNITS);
        int64_t note_measureend = (
            (int32_t)(noteref->munitoffset +
            noteref->munitlen)
            / (int32_t)MIDMUS_MEASUREUNITS) + bl->measurestart;
        int64_t note_measureoffsetend = (
            (int64_t)(noteref->munitoffset +
            noteref->munitlen) - (
            (int64_t)MIDMUS_MEASUREUNITS *
            (note_measureend - (int64_t)bl->measurestart)));
        int64_t frameendoffsetinblock = (
            _framestartofmeasure(bl->parent->parent,
            note_measureend)) - _framestartofmeasure(
            bl->parent->parent, bl->measurestart) +
            ((int64_t)
            bl->parent->parent->measure[note_measureend].
            framelen * (int64_t)note_measureoffsetend /
            (int64_t)MIDMUS_MEASUREUNITS);
        noteref->framelen = (
            frameendoffsetinblock -
            noteref->frameoffsetinblock);
        i++;
    }
}


void midmussong_UpdateModifierNextMUnitAndTime(
        midmusblock *bl) {
    int k = 0;
    while (k < bl->modifiercount) {
        int32_t startmeasure = (bl->modifier[k].munitoffset /
            (int64_t)MIDMUS_MEASUREUNITS) +
            (int64_t)bl->measurestart;
        bl->modifier[k].frameoffsetinblock = (
            _framestartofmeasure(bl->parent->parent,
            startmeasure) - _framestartofmeasure(
            bl->parent->parent, bl->measurestart));
        int64_t offset_in_measure = (
            bl->modifier[k].munitoffset -
            (int64_t)(startmeasure - bl->measurestart) *
            (int64_t)MIDMUS_MEASUREUNITS);
        assert(offset_in_measure >= 0);
        assert(offset_in_measure < MIDMUS_MEASUREUNITS);
        bl->modifier[k].frameoffsetinblock += (
            bl->parent->parent->measure[startmeasure].
            framelen *
            offset_in_measure / (int64_t)MIDMUS_MEASUREUNITS);
        assert(bl->modifier[k].frameoffsetinblock <=
            _framestartofmeasure(bl->parent->parent,
            startmeasure) - _framestartofmeasure(
            bl->parent->parent, bl->measurestart) +
            bl->parent->parent->measure[startmeasure].
            framelen);
        k++;
    }
    k = 0;
    while (k < bl->modifiercount) {
        int type = bl->modifier[k].type;
        bl->modifier[k].nextsametypemunitoffset = -1;
        bl->modifier[k].nextsametypeframeoffset = -1;
        int k2 = k + 1;
        while (k2 < bl->modifiercount) {
            if (bl->modifier[k2].type == type) {
                assert(bl->modifier[k2].munitoffset >=
                    bl->modifier[k].munitoffset);
                assert(bl->modifier[k2].frameoffsetinblock >=
                    bl->modifier[k].frameoffsetinblock);
                bl->modifier[k].nextsametypemunitoffset =
                    bl->modifier[k2].munitoffset;
                bl->modifier[k].nextsametypeframeoffset =
                    bl->modifier[k2].frameoffsetinblock;
                break;
            }
            k2++;
        }
        assert(bl->modifier[k].nextsametypemunitoffset == -1 ||
               bl->modifier[k].nextsametypemunitoffset >=
               bl->modifier[k].munitoffset);
        assert(bl->modifier[k].nextsametypeframeoffset == -1 ||
               bl->modifier[k].nextsametypeframeoffset >=
               bl->modifier[k].frameoffsetinblock);
        k++;
    }
}


int midmussong_TransferModifierToBlock(
        midmusblock *bl, midmusmodify *mod,
        int modifierhasglobaltime, int *out_hadoom) {
    midmusmodify copy;
    memcpy(&copy, mod, sizeof(copy));
    if (modifierhasglobaltime) {
        copy.munitoffset -= (
            (int64_t)(MIDMUS_MEASUREUNITS) *
            (int64_t)bl->measurestart);
    }
    if (copy.munitoffset < 0 ||
            copy.munitoffset >
            (int64_t)(MIDMUS_MEASUREUNITS) *
            (int64_t)bl->measurelen) {
        *out_hadoom = 0;
        return 0;
    }
    midmusmodify *newmodifier = realloc(
        bl->modifier, sizeof(*newmodifier) *
        (bl->modifiercount + 1));
    if (!newmodifier) {
        *out_hadoom = 1;
        return 0;
    }
    bl->modifier = newmodifier;
    memcpy(&bl->modifier[bl->modifiercount],
        &copy, sizeof(copy));
    bl->modifiercount++;
    return 1;
}


void midmussong_MeasureTimeSig(
        midmussong *song, int measure,
        int32_t *out_signom, int32_t *out_sigdiv,
        double *out_beatspermeasure) {
    if (song->measurecount <= 0) {
        *out_signom = 4;
        *out_sigdiv = 4;
        *out_beatspermeasure = 4;
        return;
    }
    if (measure < 0) measure = 0;
    if (measure >= song->measurecount) measure = song->measurecount - 1;
    if (out_signom)
        *out_signom = song->measure[measure].signaturenom;
    if (out_sigdiv)
        *out_sigdiv = song->measure[measure].signaturediv;
    if (out_beatspermeasure)
        *out_beatspermeasure = (
            song->measure[measure].beatspermeasure);
}



static int _midmus_MidiSourceModifierValueAtMUnit(
        midmussong_midiinfo *minfo,
        int type, int sourcetrack, int sourcechan,
        int64_t munit, int64_t *out_lastmodmunitoffset
        ) {
    assert(sourcetrack >= 0 &&
        sourcetrack < minfo->sourcetrackcount);
    assert(sourcechan >= 0 && sourcechan < SMF_MAX_CHANNELS);
    midmussong_midiinfosourcechannel *c = &(
        minfo->sourcetrackinfo[sourcetrack].channel[sourcechan]);
    int val = -1;
    int k = 0;
    while (k < c->modeventcount) {
        if (c->modevent[k].type != type) {
            k++;
            continue;
        }
        if (c->modevent[k].munitoffset <= munit) {
            *out_lastmodmunitoffset = (
                c->modevent[k].munitoffset);
            if (*out_lastmodmunitoffset < 0)
                *out_lastmodmunitoffset = 0;
            val = c->modevent[k].value;
            val = (val < 0 ? 0 : val);
        } else if (c->modevent[k].munitoffset >
                munit) {
            break;
        }
        k++;
    }
    if (val >= 0)
        return val;
    *out_lastmodmunitoffset = -1;
    if (type == MIDMUSMODIFY_VOL)
        return 100;  // Note: General MIDI 2 default value.
                     // If changed, midi importer needs adjustment.
    if (type == MIDMUSMODIFY_PAN)
        return 64;
    if (type == MIDMUSMODIFY_PITCH)
        return 64;
    return 127;
}


int _midmus_FinalizeMidiSourceModifiers(
        midmussong_midiinfo *minfo) {
    // Ensure every _MIDMUSMODIFY_TEMP_MIDIEXPR has a
    // corresponding MIDMUSMODIFY_VOL event:
    int track = 0;
    while (track < minfo->sourcetrackcount) {
        midmussong_midiinfosourcetrack *t = &(
            minfo->sourcetrackinfo[track]);
        int channel = 0;
        while (channel < SMF_MAX_CHANNELS) {
            midmussong_midiinfosourcechannel *c = &(
                t->channel[channel]);
            int i = 0;
            while (i < c->modeventcount) {
                if (c->modevent[i].type !=
                        _MIDMUSMODIFY_TEMP_MIDIEXPR) {
                    i++;
                    continue;
                }
                int64_t munit = -1;
                int val = _midmus_MidiSourceModifierValueAtMUnit(
                    minfo, MIDMUSMODIFY_VOL, track, channel,
                    c->modevent[i].munitoffset, &munit);
                if (munit == c->modevent[i].munitoffset && munit >= 0) {
                    // We already have 1+ corresponding VOL event(s)
                    i++;
                    continue;
                }
                assert(munit < c->modevent[i].munitoffset);
                int32_t oldmodcount = c->modeventcount;
                midmusmodify *modeventnew = realloc(
                    c->modevent,
                    sizeof(*modeventnew) * (oldmodcount + 1));
                if (!modeventnew)
                    return 0;
                c->modevent = modeventnew;
                c->modeventcount++;
                memmove(&c->modevent[i + 1],
                    &c->modevent[i],
                    sizeof(*c->modevent) * (oldmodcount - i));
                memset(&c->modevent[i], 0, sizeof(*c->modevent));
                c->modevent[i].type = MIDMUSMODIFY_VOL;
                c->modevent[i].value = val;
                c->modevent[i].munitoffset =
                    c->modevent[i + 1].munitoffset;
                i += 2;
            }
            channel++;
        }
        track++;
    }

    // Adjust every MIDMUSMODIFY_VOL event by the
    // applicable _MIDMUSMODIFY_TEMP_MIDIEXPR value:
    track = 0;
    while (track < minfo->sourcetrackcount) {
        midmussong_midiinfosourcetrack *t = &(
            minfo->sourcetrackinfo[track]);
        int channel = 0;
        while (channel < SMF_MAX_CHANNELS) {
            midmussong_midiinfosourcechannel *c = &(
                t->channel[channel]);
            int i = 0;
            while (i < c->modeventcount) {
                if (c->modevent[i].type !=
                        MIDMUSMODIFY_VOL) {
                    i++;
                    continue;
                }
                int64_t munit = -1;
                int32_t expr = _midmus_MidiSourceModifierValueAtMUnit(
                    minfo, _MIDMUSMODIFY_TEMP_MIDIEXPR, track,
                    channel, c->modevent[i].munitoffset, &munit);
                assert(munit <= c->modevent[i].munitoffset);
                assert(expr >= 0 && expr <= 127);
                int32_t val = (
                    c->modevent[i].value * (expr * expr) / (127 * 127));
                assert(val >= 0);
                val = (val < 127 ? val : 127);
                c->modevent[i].value = val;
                i++;
            }
            channel++;
        }
        track++;
    }

    // Dump all _MIDMUSMODIFY_TEMP_MIDIEXPR events after baking:
    track = 0;
    while (track < minfo->sourcetrackcount) {
        midmussong_midiinfosourcetrack *t = &(
            minfo->sourcetrackinfo[track]);
        int channel = 0;
        while (channel < SMF_MAX_CHANNELS) {
            midmussong_midiinfosourcechannel *c = &(
                t->channel[channel]);
            int i = 0;
            while (i < c->modeventcount) {
                if (c->modevent[i].type !=
                        _MIDMUSMODIFY_TEMP_MIDIEXPR) {
                    i++;
                    continue;
                }
                if (i + 1 < c->modeventcount)
                    memmove(&c->modevent[i],
                        &c->modevent[i + 1],
                        sizeof(*c->modevent) * (
                        c->modeventcount - i - 1));
                c->modeventcount--;
                // No i++ here.
                continue;
            }
            channel++;
        }
        track++;
    }
    return 1;
}


midmussong *midmussong_Load(
        const char *bytes, int64_t byteslen) {
    midmussong *song = malloc(sizeof(*song));
    if (!song)
        return NULL;
    memset(song, 0, sizeof(*song));
    midmussong_midiinfo minfo = {0};
    minfo.song = song;

    struct midi_parser parser = {0};
    parser.state = MIDI_PARSER_INIT;
    parser.size = byteslen;
    parser.in = (uint8_t *)bytes;
    #if defined(DEBUG_MIDIPARSER)
    printf("rfsc/midi/midmus.c: debug: "
        "parser init (%" PRId64
        " bytes midi, song %p)...\n",
        byteslen, song);
    #endif

    enum midi_parser_status status;

    // Before anything else, go through song to extract
    // time signature changes (otherwise things will break
    // if they aren't on the very first track):
    int seen_note_on = 0;
    int seen_note_off_past_on = 0;
    int current_track = -1;
    int current_time = 0;
    int eof = 0;
    while (!eof) {
        status = midi_parse(&parser);
        switch (status) {
        case MIDI_PARSER_EOB:
            eof = 1;
            break;
        case MIDI_PARSER_ERROR: {
            if (seen_note_on && seen_note_off_past_on) {
                // Treat this as EOF and play what we have.
                eof = 1;
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: warning: "
                    "ignoring corrupt file ending\n");
                #endif
                break;
            }
            #ifdef DEBUG_MIDIPARSER
            printf("rfsc/midi/midmus.c: error: "
                "file parse error (in time signature "
                "pre-parse)\n");
            #endif
            midmussong_FreeMidiInfoContents(&minfo);
            midmussong_Free(song);
            return NULL;
        }
        case MIDI_PARSER_INIT:
            break;
        case MIDI_PARSER_HEADER: {
            if ((parser.header.time_division & 0x8000) != 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "SMPTE timing not supported\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            minfo.midippq = parser.header.time_division;
            if (minfo.midippq <= 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "invalid midi clocks per beat in song "
                    "header\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            break;
        }
        case MIDI_PARSER_TRACK: {
            current_time = 0;
            current_track++;
            break;
        }
        case MIDI_PARSER_TRACK_MIDI: {
            if (current_track < 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "invalid midi event outside of track\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            int abstime = current_time + (int)parser.vtime;
            if (parser.midi.channel < 0 || parser.midi.channel >= 16) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "midi event with invalid channel %d\n",
                    parser.midi.channel);
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            int channel = parser.midi.channel;
            if (parser.midi.status == MIDI_STATUS_NOTE_ON) {
                if (parser.midi.param2 == 0) {
                    if (seen_note_on)
                        seen_note_off_past_on = 1;
                } else {
                    seen_note_on = 1;
                }
            } else if (parser.midi.status == MIDI_STATUS_NOTE_OFF) {
                if (seen_note_on)
                    seen_note_off_past_on = 1;
            }
            current_time += parser.vtime;
            break;
        }
        case MIDI_PARSER_TRACK_META: {
            current_time += parser.vtime;
            if (parser.meta.type ==
                    MIDI_META_TIME_SIGNATURE) {
                if (parser.meta.length < 2) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "time signature event with "
                        "invalid length\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                int nominator = (
                    (uint8_t *)parser.meta.bytes)[0];
                int divisor_pow2 = (
                    (uint8_t *)parser.meta.bytes)[1];
                int divisor = 1;
                if (divisor_pow2 >= 1) {
                    divisor = 2;
                    int i = 2;
                    while (i <= divisor_pow2) {
                        divisor *= 2;
                        i++;
                    }
                }
                if (nominator <= 0 || divisor > 256) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: warning: "
                        "time signature event with "
                        "invalid signature values\n");
                    #endif
                    nominator = 4;
                    divisor = 4;
                }
                if (!_midmussong_KeepMidiTimeSigChange(
                        &minfo,
                        current_time - parser.vtime,
                        nominator, divisor)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: warning: "
                        "failed to store time signature event, "
                        "out of memory?\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
            }
            break;
        }
        case MIDI_PARSER_TRACK_SYSEX: {
            current_time += parser.vtime;
            break;
        }
        default:
            break;
        }
    }

    // Feed time signature changes so midi clock times will be
    // correctly translated to measure position in next parse
    // loops:
    int i = 0;
    while (i < minfo.timesigchangecount) {
        int32_t measure;
        double place_in_measure;
        if (!_miditimetomeasure(&minfo,
                minfo.timesigchange[i].midiclock,
                &measure, &place_in_measure)) {
            #ifdef DEBUG_MIDIPARSER
            printf("rfsc/midi/midmus.c: error: "
                "failed to compute time sig change measure, oom?\n");
            #endif
            midmussong_FreeMidiInfoContents(&minfo);
            midmussong_Free(song);
            return NULL;
        }
        midmussong_SetMeasureTimeSig(
            minfo.song, measure,
            minfo.timesigchange[i].signom,
            minfo.timesigchange[i].sigdiv);
        i++;
    }

    #ifdef DEBUG_MIDIPARSER_TIMESIG
    i = 0;
    while (i < minfo.song->measurecount) {
        printf("rfsc/midi/midmus.c: debug: "
            "measure %d/%d timesig %d/%d midi ticks len %d "
            "beatspermeasure %f\n",
            i, minfo.song->measurecount,
            minfo.song->measure[i].signaturenom,
            minfo.song->measure[i].signaturediv,
            (int)_miditicksofmeasure(&minfo, i),
            minfo.song->measure[i].beatspermeasure);
        i++;
    }
    #endif

    // Then, pre-parse all modifier events:
    memset(&parser, 0, sizeof(parser));
    parser.state = MIDI_PARSER_INIT;
    parser.size = byteslen;
    parser.in = (uint8_t *)bytes;
    seen_note_on = 0;
    seen_note_off_past_on = 0;
    current_track = -1;
    current_time = 0;
    eof = 0;
    while (!eof) {
        status = midi_parse(&parser);
        switch (status) {
        case MIDI_PARSER_EOB:
            eof = 1;
            break;
        case MIDI_PARSER_ERROR: {
            assert(seen_note_on && seen_note_off_past_on);
            eof = 1;
            break;
        }
        case MIDI_PARSER_INIT:
            break;
        case MIDI_PARSER_HEADER: {
            break;
        }
        case MIDI_PARSER_TRACK: {
            current_time = 0;
            current_track++;
            break;
        }
        case MIDI_PARSER_TRACK_MIDI: {
            assert(current_track >= 0);  // verified in time sig loop
            int abstime = current_time + (int)parser.vtime;
            int32_t measure;
            double place_in_measure;
            if (!_miditimetomeasure(&minfo, abstime,
                    &measure, &place_in_measure)) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "failed to compute note on measure, oom?\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            assert(parser.midi.channel >= 0 &&
                parser.midi.channel < 16);  // verified in time sig loop
            int channel = parser.midi.channel;
            if ((parser.midi.status == MIDI_STATUS_CC &&
                    (parser.midi.param1 == 7 ||  // volume
                    parser.midi.param1 == 10 ||  // pan
                    parser.midi.param1 == 11)) ||  // midi expression
                    parser.midi.status == MIDI_STATUS_PITCH_BEND
                    ) {
                if (!midmussong_EnsureSourceTrackInfo(&minfo,
                        current_track)) {
                    oommodevent: ;
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "out of memory handling modifier event "
                        "on source track %d, channel %d\n",
                        current_track, parser.midi.channel);
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                int oldmodcount = (
                    minfo.sourcetrackinfo[current_track].
                        channel[channel].modeventcount);
                midmusmodify *modeventnew = realloc(
                    minfo.sourcetrackinfo[current_track].
                    channel[channel].modevent,
                    sizeof(*modeventnew) * (oldmodcount + 1));
                if (!modeventnew)
                    goto oommodevent;
                memset(&modeventnew[oldmodcount], 0,
                    sizeof(*modeventnew));
                modeventnew[oldmodcount].type =
                    (parser.midi.status == MIDI_STATUS_CC ? (
                    (parser.midi.param1 == 7 ? MIDMUSMODIFY_VOL :
                    (parser.midi.param1 == 10 ? MIDMUSMODIFY_PAN :
                    (parser.midi.param1 == 11 ?
                     _MIDMUSMODIFY_TEMP_MIDIEXPR : 0)))) :
                    MIDMUSMODIFY_PITCH);
                if (parser.midi.status == MIDI_STATUS_CC) {
                    int v = (uint8_t)parser.midi.param2;
                    v = (v >= 0 ? (v <= 127 ? v : 127) : 0);
                    modeventnew[oldmodcount].value = v;
                } else {
                    assert(modeventnew[oldmodcount].type ==
                        MIDMUSMODIFY_PITCH);
                    int v = ((((uint16_t)parser.midi.param2) << 7) +
                        (((uint16_t)parser.midi.param1) &
                        (uint16_t)0x7F)) / 128;
                    v = (v >= 0 ? (v <= 127 ? v : 127) : 0);
                    modeventnew[oldmodcount].value = v;
                }
                modeventnew[oldmodcount].munitoffset =
                    (int64_t)measure * (int64_t)MIDMUS_MEASUREUNITS +
                    (int64_t)roundl((double)MIDMUS_MEASUREUNITS *
                        fmin(1, place_in_measure));
                assert(oldmodcount <= 0 ||
                    modeventnew[oldmodcount - 1].munitoffset <=
                    modeventnew[oldmodcount].munitoffset);
                minfo.sourcetrackinfo
                    [current_track].channel[channel].
                    modeventcount++;
                minfo.sourcetrackinfo
                    [current_track].channel[channel].modevent =
                    modeventnew;
            } else if (parser.midi.status == MIDI_STATUS_NOTE_ON) {
                if (parser.midi.param2 == 0) {
                    if (seen_note_on)
                        seen_note_off_past_on = 1;
                } else {
                    seen_note_on = 1;
                }
            } else if (parser.midi.status == MIDI_STATUS_NOTE_OFF) {
                if (seen_note_on)
                    seen_note_off_past_on = 1;
            }
            current_time += parser.vtime;
            break;
        }
        case MIDI_PARSER_TRACK_META: {
            current_time += parser.vtime;
            break;
        }
        case MIDI_PARSER_TRACK_SYSEX: {
            current_time += parser.vtime;
            break;
        }
        default:
            break;
        }
    }

    // Clean up modifier events, and process midi expression:
    if (!_midmus_FinalizeMidiSourceModifiers(&minfo)) {
        #ifdef DEBUG_MIDIPARSER
        printf("rfsc/midi/midmus.c: error: "
            "out of memory finalizing modifiers\n",
            current_track, parser.midi.channel);
        #endif
        midmussong_FreeMidiInfoContents(&minfo);
        midmussong_Free(song);
        return NULL;
    }

    // Actual main parse run for everything else (notes, etc.):
    memset(&parser, 0, sizeof(parser));
    parser.state = MIDI_PARSER_INIT;
    parser.size = byteslen;
    parser.in = (uint8_t *)bytes;
    current_track = -1;
    current_time = 0;
    seen_note_on = 0;
    seen_note_off_past_on = 0;
    eof = 0;
    while (!eof) {
        status = midi_parse(&parser);
        switch (status) {
        case MIDI_PARSER_EOB:
            eof = 1;
            break;
        case MIDI_PARSER_ERROR:
            assert(seen_note_on && seen_note_off_past_on);
            eof = 1;
            break;
        case MIDI_PARSER_INIT:
            break;
        case MIDI_PARSER_HEADER:
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("header\n");
            printf("  size: %d\n", parser.header.size);
            printf("  format: %d [%s]\n", parser.header.format,
                midi_file_format_name(parser.header.format));
            printf("  tracks count: %d\n", parser.header.tracks_count);
            printf("  time division: %d\n", parser.header.time_division);
            #endif
            break;
        case MIDI_PARSER_TRACK:
            current_time = 0;
            current_track++;
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track %d start\n", current_track);
            printf("  length: %d\n", parser.track.size);
            #endif
            break;
        case MIDI_PARSER_TRACK_MIDI: {
            assert(current_track >= 0);  // verified in time sig loop
            assert(parser.midi.channel >= 0 &&
                parser.midi.channel < 16);  // verified in time sig loop
            int abstime = current_time + (int)parser.vtime;
            int32_t measure;
            double place_in_measure;
            if (!_miditimetomeasure(&minfo, abstime,
                    &measure, &place_in_measure)) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "failed to compute note on measure, oom?\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track %d event\n", current_track);
            printf("  time: %d (%d)\n", abstime, (int)parser.vtime);
            printf("  status: %u [%s]\n",
                (unsigned int)(parser.midi.status),
                midi_status_name(parser.midi.status));
            printf("  channel: %d\n", parser.midi.channel);
            printf("  param1: %d\n", parser.midi.param1);
            printf("  param2: %d\n", parser.midi.param2);
            printf("  measure: %d\n", measure);
            printf("  place in measure: %f\n", place_in_measure);
            #endif
            if (parser.midi.channel == 9) {
                minfo.sourcetrackinfo[current_track].
                    channel[parser.midi.channel].
                    midiinstrument = 129;  // drums
            }
            if (parser.midi.status == MIDI_STATUS_NOTE_ON) {
                if (parser.midi.param2 == 0) {
                    if (seen_note_on)
                        seen_note_off_past_on = 1;
                } else {
                    seen_note_on = 1;
                }
                if (!midmussong_FeedMidiNoteOn(
                        &minfo, current_track, parser.midi.channel,
                        measure, place_in_measure,
                        parser.midi.param1, parser.midi.param2)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "failed to process note on, oom?\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
            } else if (parser.midi.status == MIDI_STATUS_NOTE_OFF) {
                if (seen_note_on)
                    seen_note_off_past_on = 1;
                if (!midmussong_FeedMidiNoteOff(
                        &minfo, current_track, parser.midi.channel,
                        parser.midi.param1, measure, place_in_measure)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "failed to process note off, oom?\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
            } else if (parser.midi.status == MIDI_STATUS_PGM_CHANGE) {
                assert(current_track >= 0);
                if (!midmussong_EnsureSourceTrackInfo(
                        &minfo, current_track)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "oom processing program change\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                assert(minfo.sourcetrackcount >= current_track + 1);
                assert(minfo.sourcetrackinfo != NULL);
                int inst = parser.midi.param1 + 1;
                if (inst < 1 || inst > 128) {
                    // No valid instrument...?
                    inst = -1;
                }
                if (parser.midi.channel != 9)  // (not drums track)
                    minfo.sourcetrackinfo[current_track].
                        channel[parser.midi.channel].midiinstrument =
                        inst;
                if (!midmussong_FeedMidiAllNotesOff(
                        &minfo, current_track, parser.midi.channel,
                        measure, place_in_measure
                        )) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "oom processing all notes off for program change\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
            }
            current_time += parser.vtime;
            break;
        }
        case MIDI_PARSER_TRACK_META: {
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track-meta\n");
            printf("  time: %ld\n", parser.vtime);
            printf("  type: %d [%s]\n", parser.meta.type,
                midi_meta_name(parser.meta.type));
            printf("  length: %d\n", parser.meta.length);
            #endif
            int abstime = current_time + (int)parser.vtime;
            int32_t measure;
            double place_in_measure;
            if (!_miditimetomeasure(&minfo, abstime,
                    &measure, &place_in_measure)) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midmus.c: error: "
                    "failed to compute note on measure, oom?\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            if (parser.meta.type ==
                    MIDI_META_SET_TEMPO) {
                if (parser.meta.length < 1) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "set tempo event with invalid length\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                int len = parser.meta.length;
                uint32_t microseconds = (
                    ((uint8_t *)parser.meta.bytes)[len - 1]);
                if (len >= 2)
                    microseconds += ((uint32_t)(
                        ((uint8_t *)parser.meta.bytes)[len - 2])) << 8;
                if (len >= 3)
                    microseconds += ((uint32_t)(
                        ((uint8_t *)parser.meta.bytes)[len - 3])) << 16;
                double bpm = 60000000.0 / (double)microseconds;
                if (!midmussong_EnsureMeasureCount(
                        minfo.song, measure)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "failed to set bpm on measure, oom?\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                midmussong_SetMeasureBPM(
                    minfo.song, measure, bpm);
            }
            current_time += parser.vtime;
            break;
        }
        case MIDI_PARSER_TRACK_SYSEX:
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track-sysex");
            printf("  time: %ld\n", parser.vtime);
            #endif
            current_time += parser.vtime;
            break;
        default:
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("unhandled state: %d\n", status);
            #endif
            break;
        }
    }

    // Make sure timing is set on all final notes:
    int k = 0;
    while (k < song->trackcount) {
        int j = 0;
        while (j < song->track[k].blockcount) {
            midmussong_UpdateBlockSamplePos(
                &song->track[k].block[j]);
            j++;
        }
        k++;
    }

    // Calculate all the overlap indexes for playback:
    k = 0;
    while (k < song->trackcount) {
        int j = 0;
        while (j < song->track[k].blockcount) {
            midmussong_RecalculateBlockNoteOverlaps(
                &song->track[k].block[j]);
            j++;
        }
        k++;
    }

    // Move over the modifiers for volume, pan, etc:
    k = 0;
    while (k < song->trackcount) {
        int srctrack = minfo.songtrackinfo[k].midisourcetrack;
        int chan = minfo.songtrackinfo[k].
            midiorigfirstsourcechannel;
        if (song->track[k].blockcount < 1 ||
                song->track[k].block[0].modifiercount > 0) {
            k++;
            continue;
        }
        // Find the block for each modifier:
        int j = 0;
        while (j < minfo.sourcetrackinfo[srctrack].
                channel[chan].modeventcount) {
            int z = 0;
            while (z < song->track[k].blockcount) {
                int hadoom = 0;
                if (midmussong_TransferModifierToBlock(
                        &song->track[k].block[z],
                        &minfo.sourcetrackinfo[srctrack].
                            channel[chan].modevent[j],
                        1, &hadoom))
                    break;
                if (hadoom) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midmus.c: error: "
                        "out of memory transfering "
                        "modifiers\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                z++;
            }
            j++;
        }
        // Now dump old modifier list after our transfer:
        free(minfo.sourcetrackinfo[srctrack].
            channel[chan].modevent);
        minfo.sourcetrackinfo[srctrack].
            channel[chan].modevent = NULL;
        minfo.sourcetrackinfo[srctrack].
            channel[chan].modeventcount = 0;
        k++;
    }

    // Update modifiers referencing their follow-up ones:
    k = 0;
    while (k < song->trackcount) {
        int j = 0;
        while (j < song->track[k].blockcount) {
            midmussong_UpdateModifierNextMUnitAndTime(
                &song->track[k].block[j]);
            j++;
        }
        k++;
    }

    #ifdef DEBUG_MIDIPARSER
    printf("rfsc/midi/midmus.c: debug: "
        "finished parsing song with %d tracks", song->trackcount);
    if (song->trackcount > 0) {
        printf(" (");
        k = 0;
        while (k < song->trackcount) {
            if (k > 0)
                printf(", ");
            printf("#%d %s (from midi track #%d)", k + 1,
                midmussong_InstrumentNoToName(
                song->track[k].instrument),
                (int)minfo.songtrackinfo[k].midisourcetrack);
            k++;
        }
    }
    printf("\n");
    #endif

    // Free extra midi tracking data we no longer need:
    midmussong_FreeMidiInfoContents(&minfo);

    return song;
}


void midmussong_Free(midmussong *song) {
    if (!song)
        return;

    int k = 0;
    while (k < song->trackcount) {
        int j = 0;
        while (j < song->track[k].blockcount) {
            free(song->track[k].block[j].modifier);
            free(song->track[k].block[j].note);
            j++;
        }
        free(song->track[k].block);
        k++;
    }
    free(song->track);
    free(song->measure);
    free(song);
}


static char *gm_instnames[] = {
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1",
    "Electric Piano 2",
    "Harpsichord",
    "Clavinet",
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    "Drawbar Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar harmonics",
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    "String Ensemble 1",
    "String Ensemble 2",
    "Synth Strings 1",
    "Synth Strings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "Synth Brass 1",
    "Synth Brass 2",
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope)",
    "Lead 4 (chiff)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bag pipe",
    "Fiddle",
    "Shanai",
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot",
    "Drums",
    "Unknown",
    NULL
};

const char *midmussong_InstrumentNoToName(int instrno) {
    if (instrno < 1 || instrno > 129)
        return gm_instnames[129];
    return gm_instnames[instrno - 1];
}


static char *gm_drumnames[] = {
    "High Q (GM2)",  // starts at midi "key" 27
    "Slap (GM2)",
    "Scratch Push (GM2)",
    "Scratch Pull (GM2)",
    "Sticks (GM2)",
    "Square Click (GM2)",
    "Metronome Click (GM2)",
    "Metronome Bell (GM2)",
    "Bass Drum 2",
    "Bass Drum 1",
    "Side Stick",
    "Snare Drum 1",
    "Hand Clap",
    "Snare Drum 2",
    "Low Tom 2",
    "Closed Hi-hat",
    "Low Tom 1",
    "Pedal Hi-hat",
    "Mid Tom 2",
    "Open Hi-hat",
    "Mid Tom 1",
    "High Tom 2",
    "Crash Cymbal 1",
    "High Tom 1",
    "Ride Cymbal 1",
    "Chinese Cymbal",
    "Ride Bell",
    "Tambourine",
    "Splash Cymbal",
    "Cowbell",
    "Crash Cymbal 2",
    "Vibra Slap",
    "Ride Cymbal 2",
    "High Bongo",
    "Low Bongo",
    "Mute High Conga",
    "Open High Conga",
    "Low Conga",
    "High Timbale",
    "Low Timbale",
    "High Agogo",
    "Low Agogo",
    "Cabasa",
    "Maracas",
    "Short Whistle",
    "Long Whistle",
    "Short Guiro",
    "Long Guiro",
    "Claves",
    "High Wood Block",
    "Low Wood Block",
    "Mute Cuica",
    "Open Cuica",
    "Mute Triangle",
    "Open Triangle",
    "Shaker (GM2)",
    "Jingle Bell (GM2)",
    "Belltree (GM2)",
    "Castanets (GM2)",
    "Mute Surdo (GM2)",
    "Open Surdo (GM2)",
    "Unknown",
    NULL
};

const char *midmussong_DrumKeyToName(int keyno) {
    if (keyno < 27 || keyno > 87)
        return gm_instnames[60];
    return gm_drumnames[keyno - 27];
}
