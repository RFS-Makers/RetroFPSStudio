// This is PROPRIETARY CODE, do not modify, reuse, or share.
// All Rights Reserved.
// Reading this code for personal education and curiosity is ENCOURAGED!
// See LICENSE.md for details

#ifndef RFS2_AUDIO_H_
#define RFS2_AUDIO_H_

#include <stdint.h>

typedef struct h3daudiodevice h3daudiodevice;
typedef struct h3daudiodecodedfile h3daudiodecodedfile;
typedef struct rfssound rfssound;


#define H3DAUDIO_BACKEND_DEFAULT 0
#define H3DAUDIO_BACKEND_SDL2 1
#define H3DAUDIO_BACKEND_SDL2_EXCLUSIVELOWLATENCY 2
#define H3DAUDIO_BACKEND_MINIAUDIO 3

#define MIXTYPE int16_t

h3daudiodevice *h3daudio_OpenDeviceEx(
    int samplerate, int audiobufsize,
    int backend, const char *soundcardname,
    char **error
);

void h3daudio_DoForAllDevices(
    void (*callback)(h3daudiodevice *dev, void *userdata),
    void *userdata);

h3daudiodevice *h3daudio_OpenDevice(
    char **error
);

int h3daudio_GetDeviceSoundcardCount(
    int backendtype
);

char *h3daudio_GetDeviceSoundcardName(
    int backendtype, int soundcardindex
);

int h3daudio_GetDeviceSampleRate(h3daudiodevice *dev);

void h3daudio_DestroyDevice(h3daudiodevice *dev);

const char *h3daudio_GetDeviceName(h3daudiodevice *dev);

uint64_t h3daudio_PlayStreamedSoundFromFile(
    h3daudiodevice *dev, const char *path,
    double volume, double panning, int loop
);

uint64_t h3daudio_PlayPredecodedSound(
    h3daudiodevice *dev, h3daudiodecodedfile *f,
    double volume, double panning, int loop
);

uint64_t h3daudio_PlayCallbackSrcSound(
    h3daudiodevice *dev,
    int (*callback_read)(void *userdata,
        void *buf, int bytes, int *haderror),
    int (*callback_tostart)(void *userdata),
    void (*callback_close)(void *userdata),
    void *callback_userdata,
    double volume, double panning, int loop
);

void h3daudio_StopAllSoundsWithCallbackAndUserdata(
    h3daudiodevice *dev,
    int (*callback_read)(void *userdata,
        void *buf, int bytes, int *haderror),
    int (*callback_tostart)(void *userdata),
    void (*callback_close)(void *userdata),
    void *callback_userdata);

int h3daudio_HasSoundsWithCallbackAndUserdata(
    h3daudiodevice *dev,
    int (*callback_read)(void *userdata,
        void *buf, int bytes, int *haderror),
    int (*callback_tostart)(void *userdata),
    void (*callback_close)(void *userdata),
    void *callback_userdata);

int h3daudio_IsSoundPlaying(
    h3daudiodevice *dev, uint64_t id
);

int h3daudio_SoundHadPlaybackError(
    h3daudiodevice *dev, uint64_t id
);

void h3daudio_ChangeSoundVolume(
    h3daudiodevice *dev, uint64_t id,
    double volume, double panning
);

int h3daudio_GetSoundVolume(
    h3daudiodevice *dev, uint64_t id,
    double *volume, double *panning
);

void h3daudio_StopSound(
    h3daudiodevice *dev, uint64_t id
);

int h3daudio_GetDeviceId(h3daudiodevice *dev);

h3daudiodevice *h3daudio_GetDeviceById(int id);

void h3daudio_StopUsingPredecodedSound(h3daudiodecodedfile *f);

char *audio_FixSfxPath(const char *path);

#endif  // #ifndef RFS2_AUDIO_H_
