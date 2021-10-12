// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#include "compileconfig.h"

#include <assert.h>
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
    int startmeasure;
    double startmeasurepos;
} midmussong_midiinfotrackplayingnote;

typedef struct midmussong_midiinfotrack {
    int midiinstrument;
    int midisourcetrack;

    int playingnotecount, playingnotealloc;
    midmussong_midiinfotrackplayingnote *playingnote;
} midmussong_midiinfotrack;

typedef struct midmussong_midiinfosourcetrack {
    int midiinstrument[SMF_MAX_CHANNELS];
    int pan[SMF_MAX_CHANNELS];
} midmussong_midiinfosourcetrack;

typedef struct midmussong_midiinfo {
    midmussong *song;

    midmussong_midiinfotrack *songtrackinfo;

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
    free(minfo->songtrackinfo);
    free(minfo->sourcetrackinfo);
    memset(minfo, 0, sizeof(*minfo));
}


static int midmussong_FeedMidiNoteOnToTrack(
        midmussong_midiinfo *minfo, int track,
        ATTR_UNUSED int srctrack,
        int channel, int measure, double measurepos,
        int velocity, int pan) {
    assert(velocity > 0);
    assert(pan >= -MIDMUS_PANRANGE && pan <= MIDMUS_PANRANGE);
    assert(minfo->songtrackinfo[track].midiinstrument ==
        minfo->sourcetrackinfo[srctrack].midiinstrument[channel]);
    if (minfo->song->track[track].blockcount == 0) {
        minfo->song->track[track].block = malloc(
            sizeof(*minfo->song->track[track].block));
        if (!minfo->song->track[track].block)
            return 0;
        memset(minfo->song->track[track].block, 0,
            sizeof(*minfo->song->track[track].block));
        minfo->song->track[track].block[0].parent =
            &minfo->song->track[track];
        minfo->song->track[track].blockcount = 1;
    }
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
    return 0;
}


static int midmussong_FinalizeMidiNoteOnTrack(
        midmussong_midiinfo *minfo, int track,
        ATTR_UNUSED int srctrack,
        int noteidx, int endmeasure, double endmeasurepos) {
    assert(minfo->song->track[track].blockcount > 0);
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
    insertnoteref->munitoffset = roundl(
        (double)MIDMUS_MEASUREUNITS *
        ((double)
        minfo->songtrackinfo[track].
            playingnote[noteidx].startmeasure +
        (double)minfo->songtrackinfo[track].
            playingnote[noteidx].startmeasurepos));
    int64_t endoffset = roundl(
        (double)MIDMUS_MEASUREUNITS *
        ((double)endmeasure + (double)endmeasurepos));
    insertnoteref->munitlen = (endoffset -
        insertnoteref->munitoffset);
    if (insertnoteref->munitlen < 1)
        insertnoteref->munitlen = 1;
    int vel = minfo->songtrackinfo[track].
        playingnote[noteidx].velocity;
    insertnoteref->volume = (vel <= 127 ?
        (vel >= 0 ? vel : 0) : 127);
    assert(MIDMUS_PANRANGE * 2 < 127);
    insertnoteref->pan = ((int32_t)127 +
        (int32_t)minfo->songtrackinfo[track].
        playingnote[noteidx].pan);

    // FIXME: factor in BPM here:
    /*insertnoteref->sampleoffset = ((
        insertnoteref->munitoffset * (int64_t)MIDMUS_SAMPLERATE)
        / (int64_t)MIDMUS_MEASUREUNITS);
    insertnoteref->samplelen = ((
        insertnoteref->munitlen * (int64_t)MIDMUS_SAMPLERATE)
        / (int64_t)MIDMUS_MEASUREUNITS);
    if (insertnoteref->samplelen < 1)
        insertnoteref->samplelen = 1;*/

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

    return 1;
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
                    midiinstrument[channel])) {
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


static int midmussong_FeedMidiNoteOn(
        midmussong_midiinfo *minfo, int srctrack,
        int channel, int measure, double measurepos,
        int velocity) {
    assert(srctrack >= 0);
    assert(channel >= 0 && channel < SMF_MAX_CHANNELS);
    assert(velocity >= 0);
    if (velocity > 127) velocity = 127;
    if (velocity == 0)
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
                    midiinstrument[channel])) {
            return midmussong_FeedMidiNoteOnToTrack(
                minfo, i, srctrack,
                channel, measure, measurepos, velocity,
                minfo->sourcetrackinfo[srctrack].pan[channel]);
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
        midiinstrument = minfo->sourcetrackinfo[srctrack].
            midiinstrument[channel];
    minfo->songtrackinfo[i].midisourcetrack = srctrack;
    midmustrack *newtrack = realloc(
        minfo->song->track, sizeof(*newtrack) * (
        minfo->song->trackcount + 1));
    if (!newtrack)
        return 0;
    minfo->song->track = newtrack;
    memset(&minfo->song->track[minfo->song->trackcount], 0,
        sizeof(*newtrack));
    minfo->song->trackcount++;

    // Add note to newly allocated track:
    return midmussong_FeedMidiNoteOnToTrack(
        minfo, minfo->song->trackcount - 1, srctrack,
        channel, measure, measurepos, velocity,
        minfo->sourcetrackinfo[srctrack].pan[channel]);
}


static int midmussong_FeedMidiProgramChange(
        midmussong_midiinfo *minfo, int srctrack,
        int channel, int program) {
    // Ensure allocation of info struct holding info on SOURCE track:
    // (The source is a simple midi file/official midi standard track)
    if (!midmussong_EnsureSourceTrackInfo(minfo, srctrack))
        return 0;

    if (program < 0) program = 0;
    if (program > 127) program = 127;

    minfo->sourcetrackinfo[srctrack].midiinstrument[channel] =
        program;
    return 1;
}


midmussong *midmussong_Load(const char *bytes, int byteslen) {
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
    #if defined(DEBUG_MIDIPARSER) || 1
    printf("rfsc/midi/midimus.c: debug: "
        "parser init (%d bytes midi, song %p)...\n",
        byteslen, song);
    #endif

    enum midi_parser_status status;

    int current_track = -1;
    int current_time = 0;
    int eof = 0;
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
            printf("header\n");
            printf("  size: %d\n", parser.header.size);
            printf("  format: %d [%s]\n", parser.header.format,
                midi_file_format_name(parser.header.format));
            printf("  tracks count: %d\n", parser.header.tracks_count);
            printf("  time division: %d\n", parser.header.time_division);
            if ((parser.header.time_division & 0x8000) != 0) {
                #ifdef DEBUG_MIDIPARSER
                printf("rfsc/midi/midimus.c: error: "
                    "SMPTE timing not supported\n");
                #endif
                midmussong_FreeMidiInfoContents(&minfo);
                midmussong_Free(song);
                return NULL;
            }
            break;
        case MIDI_PARSER_TRACK:
            current_time = 0;
            current_track++;
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track %d start\n", current_track);
            printf("  length: %d\n", parser.track.size);
            #endif
            break;
        case MIDI_PARSER_TRACK_MIDI:
            int abstime = current_time + (int)parser.vtime;
            int measure = (abstime /
                (parser.header.time_division * 4));
            double place_in_measure = ((double)
                (abstime % (parser.header.time_division * 4))) /
                (double)(parser.header.time_division * 4);
            #ifdef DEBUG_MIDIPARSER_EXTRA
            if (current_track == 8 || 1) {
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
            }
            #endif
            current_time += parser.vtime;
            break;
        case MIDI_PARSER_TRACK_META:
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track-meta\n");
            printf("  time: %ld\n", parser.vtime);
            printf("  type: %d [%s]\n", parser.meta.type,
                midi_meta_name(parser.meta.type));
            printf("  length: %d\n", parser.meta.length);
            #endif
            break;
        case MIDI_PARSER_TRACK_SYSEX:
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("track-sysex");
            printf("  time: %ld\n", parser.vtime);
            #endif
            break;
        default:
            #ifdef DEBUG_MIDIPARSER_EXTRA
            printf("unhandled state: %d\n", status);
            #endif
            break;
        }
    }

    midmussong_FreeMidiInfoContents(&minfo);
    return song;
}


void midmussong_Free(midmussong *song) {
    if (!song)
        return;

    free(song);
}