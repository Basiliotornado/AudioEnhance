#include <Geode/Geode.hpp>

#include <Geode/fmod/fmod.hpp>

using namespace geode::prelude;

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include <Geode/modify/System.hpp>
class $modify(FMOD::System) {
	FMOD_RESULT createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, FMOD::Sound **sound) {
		drmp3         mp3;
		unsigned int  bufferLength;
		drmp3_int64   frames;
		drmp3_int16*  buffer;
		drmp3_uint64  framesRead;
		drmp3_bool32  mp3Result;
		unsigned int  modeFlags;
		// GD doesn't use exinfo, should be safe? Idk i'm new here
		FMOD_CREATESOUNDEXINFO info;
		memset(&info, 0, sizeof(info));
		info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		
		log::debug("Creating Stream: {}", name_or_data);
		
		mp3Result = drmp3_init_file(&mp3, name_or_data, 0);
		if (!mp3Result) {
			log::debug("Could not load: {}", mp3Result);
			return FMOD::System::createStream(name_or_data, mode, exinfo, sound);
		}
		
		frames = drmp3_get_pcm_frame_count(&mp3);
		log::debug("MP3 frames: {}", frames);
		
		bufferLength = frames * sizeof(drmp3_int16) * mp3.channels;
		buffer = new drmp3_int16[bufferLength];
		
		framesRead = drmp3_read_pcm_frames_s16(&mp3, frames, buffer);
		log::debug("framesRead: {}", framesRead);
		
		info.numchannels      = 2;
		info.defaultfrequency = mp3.sampleRate;
		info.format           = FMOD_SOUND_FORMAT_PCM16;
		info.length           = bufferLength;
		
		log::debug("Creating stream from raw");
		
		modeFlags = FMOD_LOWMEM | FMOD_LOOP_NORMAL | FMOD_2D | FMOD_ACCURATETIME | FMOD_OPENMEMORY | FMOD_OPENRAW;
		FMOD_RESULT result = FMOD::System::createSound((char const*)buffer, modeFlags, &info, sound);
		log::debug("FMOD_RESULT {}", (int)result);
		free(buffer);
		return result;
	}
	
	FMOD_RESULT init(int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata) {
		FMOD_ADVANCEDSETTINGS settings;
		memset(&settings, 0, sizeof(FMOD_ADVANCEDSETTINGS));
		settings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);

		this->getAdvancedSettings(&settings);
		settings.resamplerMethod = FMOD_DSP_RESAMPLER_SPLINE;
		this->setAdvancedSettings(&settings);
		
		return FMOD::System::init(maxchannels, flags, extradriverdata);
	}
};
