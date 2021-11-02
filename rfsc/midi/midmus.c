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
    int midiinstrument;
    int midisourcetrack;
    int midiorigfirstsourcechannel;

    int playingnotecount, playingnotealloc;
    midmussong_midiinfotrackplayingnote *playingnote;
} midmussong_midiinfotrack;

typedef struct midmussong_midiinfosourcechannel {
    int32_t midiinstrument, pan;
    int32_t modeventcount;
    midmusmodify *modevent;
} midmussong_midiinfosourcechannel;

typedef struct midmussong_midiinfosourcetrack {
    midmussong_midiinfosourcechannel channel[SMF_MAX_CHANNELS];
} midmussong_midiinfosourcetrack;

typedef struct midmussong_midiinfo {
    midmussong *song;

    midmussong_midiinfotrack *songtrackinfo;

    int midippq;  // Parts Per Quarter (=clock ticks per beat)
    int sourcetrackcount;
    midmussong_midiinfosourcetrack *sourcetrackinfo;
} midmussong_midiinfo;


static void midmussong_FreeMidiInfoContents(
        midmussong_midiinfo *minfo) {
    int i = 0;
    while (i < minfo->song->trackcount) {
        free(minfo->songtrackinfo[i].playingnote);
        i++;
    }
    if (minfo->sourcetrackinfo) {
        int i = 0;
        while (i < minfo->song->trackcount) {
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
    measure->beatpermeasure = (
        (double)measure->signaturenom /
        (double)measure->signaturediv) * 4.0;
    if (fabs(measure->beatpermeasure -
            round(measure->beatpermeasure)) < 0.01 &&
            round(measure->beatpermeasure) > 0) {
        // Avoid rounding issues by erring towards whole numbers.
        measure->beatpermeasure = round(
            measure->beatpermeasure);
    }
    double beatsseconds = 1.0 / (
        (double)measure->bpm / 60.0);
    measure->framelen = round(
        ((double)MIDMUS_SAMPLERATE) * beatsseconds *
        ((double)measure->beatpermeasure));
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
        int channel, int measure, double measurepos) {
    if (minfo->song->track[track].blockcount == 0)
        return 1;  // nothing to do with this

    int i = 0;
    while (i < minfo->songtrackinfo[track].playingnotecount) {
        if (minfo->songtrackinfo[track].playingnote[i].
                srcchannel == channel) {
            return midmussong_FinalizeMidiNoteOnTrack(
                minfo, track, srctrack, i, measure, measurepos);
        }
        i++;
    }

    return 0;
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


static int midmussong_FeedMidiNoteOff(
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
        if (minfo->songtrackinfo[i].midisourcetrack == srctrack &&
                (minfo->songtrackinfo[i].midiinstrument == -1 ||
                minfo->songtrackinfo[i].midiinstrument ==
                minfo->sourcetrackinfo[srctrack].
                    channel[channel].midiinstrument)) {
            return midmussong_FeedMidiNoteOffToTrack(
                minfo, i, srctrack,
                channel, measure, measurepos);
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
            channel, measure, measurepos);

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
        minfo->song->measure[measure].beatpermeasure);
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


void midmussong_UpdateBlockSamplePos(
        midmusblock *bl) {
    if (bl->measurelen <= 0)
        bl->measurelen = 1;
    bl->frameoffset = _framestartofmeasure(
        bl->parent->parent, bl->measurestart);
    bl->framelen = _framestartofmeasure(
        bl->parent->parent, bl->measurestart +
        bl->measurelen);
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
            note_measurestart)) +
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
            note_measureend)) +
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
            startmeasure));
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
            startmeasure) +
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
    printf("rfsc/midi/midimus.c: debug: "
        "parser init (%" PRId64
        " bytes midi, song %p)...\n",
        byteslen, song);
    #endif

    enum midi_parser_status status;

    // First, pre-parse all modifier events:
    int current_track = -1;
    int current_time = 0;
    int eof = 0;
    while (!eof) {
        status = midi_parse(&parser);
        switch (status) {
        case MIDI_PARSER_EOB:
            eof = 1;
            break;
        case MIDI_PARSER_INIT:
            break;
        case MIDI_PARSER_HEADER: {
            if ((parser.header.time_division & 0x8000) != 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
                    "SMPTE timing not supported\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            minfo.midippq = parser.header.time_division;
            if (minfo.midippq <= 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
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
                printf("rfsc/midi/midimus.c: error: "
                    "invalid midi event outside of track\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            int abstime = current_time + (int)parser.vtime;
            int32_t measure;
            double place_in_measure;
            if (!_miditimetomeasure(&minfo, abstime,
                    &measure, &place_in_measure)) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
                    "failed to compute note on measure, oom?\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            if (parser.midi.channel < 0 || parser.midi.channel >= 16) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
                    "midi event with invalid channel %d\n",
                    parser.midi.channel);
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            int channel = parser.midi.channel;
            if (parser.midi.status == MIDI_STATUS_CC &&
                    parser.midi.param1 == 7) {
                if (!midmussong_EnsureSourceTrackInfo(&minfo,
                        current_track)) {
                    oommodevent: ;
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midimus.c: error: "
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
                    MIDMUSMODIFY_VOL;
                int v = parser.midi.param2;
                v = (v >= 0 ? (v <= 127 ? v : 127) : 0);
                modeventnew[oldmodcount].value = v;
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
            }
            current_time += parser.vtime;
            break;
        }
        default:
            break;
        }
    }

    // Actual main parse run for notes, times, etc.:
    memset(&parser, 0, sizeof(parser));
    parser.state = MIDI_PARSER_INIT;
    parser.size = byteslen;
    parser.in = (uint8_t *)bytes;
    current_track = -1;
    current_time = 0;
    eof = 0;
    while (!eof) {
        status = midi_parse(&parser);
        switch (status) {
        case MIDI_PARSER_EOB:
            eof = 1;
            break;
        case MIDI_PARSER_ERROR:
            #ifdef DEBUG_MIDIPARSER
            printf("rfsc/midi/midimus.c: error: "
                "file parse error\n");
            #endif
            midmussong_FreeMidiInfoContents(&minfo);
            midmussong_Free(song);
            return NULL;
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
            if (current_track < 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
                    "invalid midi event outside of track\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            int abstime = current_time + (int)parser.vtime;
            int32_t measure;
            double place_in_measure;
            if (!_miditimetomeasure(&minfo, abstime,
                    &measure, &place_in_measure)) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
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
            if (parser.midi.channel < 0 || parser.midi.channel >= 16) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
                    "midi event with invalid channel %d\n",
                    parser.midi.channel);
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            if (parser.midi.status == MIDI_STATUS_NOTE_ON) {
                if (!midmussong_FeedMidiNoteOn(
                        &minfo, current_track, parser.midi.channel,
                        measure, place_in_measure,
                        parser.midi.param1, parser.midi.param2)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midimus.c: error: "
                        "failed to process note on, oom?\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
            } else if (parser.midi.status == MIDI_STATUS_NOTE_OFF) {
                if (!midmussong_FeedMidiNoteOff(
                        &minfo, current_track, parser.midi.channel,
                        measure, place_in_measure)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midimus.c: error: "
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
                    printf("rfsc/midi/midimus.c: error: "
                        "oom processing program change\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                assert(minfo.sourcetrackcount >= current_track + 1);
                assert(minfo.sourcetrackinfo != NULL);
                int inst = parser.midi.param1;
                if (inst == 0) {
                    // No instrument...?
                    inst = -1;
                }
                minfo.sourcetrackinfo[current_track].
                    channel[parser.midi.channel].midiinstrument =
                    inst;
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
                printf("rfsc/midi/midimus.c: error: "
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
                    printf("rfsc/midi/midimus.c: error: "
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
                    printf("rfsc/midi/midimus.c: error: "
                        "failed to set bpm on measure, oom?\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                midmussong_SetMeasureBPM(
                    minfo.song, measure, bpm);
            } else if (parser.meta.type ==
                    MIDI_META_TIME_SIGNATURE) {
                if (parser.meta.length < 2) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midimus.c: error: "
                        "time signature event with "
                        "invalid length\n");
                    #endif
                    midmussong_FreeMidiInfoContents(&minfo);
                    midmussong_Free(song);
                    return NULL;
                }
                if (!midmussong_EnsureMeasureCount(
                        minfo.song, measure)) {
                    #ifdef DEBUG_MIDIPARSER
                    printf("rfsc/midi/midimus.c: error: "
                        "failed to set time signature on "
                        "measure, oom?\n");
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
                    printf("rfsc/midi/midimus.c: warning: "
                        "time signature event with "
                        "invalid signature values\n");
                    #endif
                    nominator = 4;
                    divisor = 4;
                }
                midmussong_SetMeasureTimeSig(
                    minfo.song, measure, nominator, divisor);
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
        song->track[k].block[0].modifiercount =
            minfo.sourcetrackinfo[srctrack].
            channel[chan].modeventcount;
        song->track[k].block[0].modifier =
            minfo.sourcetrackinfo[srctrack].
            channel[chan].modevent;
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
