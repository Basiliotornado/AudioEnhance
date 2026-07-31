#include <Geode/Geode.hpp>

#include <Geode/fmod/fmod.hpp>
#include <Geode/fmod/fmod_common.h>

using namespace geode::prelude;

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

FMOD_RESULT F_CALL seekBuffer(FMOD_SOUND* sound, int subsound, unsigned int pos, FMOD_TIMEUNIT postype) { 
	drmp3* mp3;
	void* pointer;
	FMOD_Sound_GetUserData(sound, &pointer);
	mp3 = (drmp3* )pointer;
	
	log::debug("Seeking to {}, {}, {}", pos, postype, subsound);
	
	drmp3_seek_to_pcm_frame(mp3, (drmp3_uint64)pos);
	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK readBuffer(FMOD_SOUND* sound, void* data, unsigned int datalen) {
	drmp3* mp3;
	void* pointer;
	FMOD_Sound_GetUserData(sound, &pointer);
	mp3 = (drmp3* )pointer;
	
	// this makes too many prints so i'm turning it off
	// log::debug("MP3: {}, {}, {}, {}, {}, {}", (void*)mp3, (void*)pointer, mp3->pcmFramesConsumedInMP3Frame, mp3->channels, mp3->sampleRate, mp3->dataSize);
	// log::debug("At {} writing {}", (void*)data, datalen);
	
	int frames = datalen / (mp3->channels * sizeof(drmp3_int16));
	drmp3_read_pcm_frames_s16(mp3, frames, (drmp3_int16* )data);
	
	// log::debug("Done");
	return FMOD_OK;
};

#include <Geode/modify/Sound.hpp>
class $modify(FMOD::Sound) {
	// i cannot believe this works
	FMOD_RESULT release() {
		drmp3* mp3;
		void* pointer;
		FMOD::Sound::getUserData(&pointer);
		
		if (pointer != nullptr) {
			mp3 = (drmp3* )pointer;
			
			log::debug("Destroying drmp3 {}", pointer);
			
			drmp3_uninit(mp3);
			delete mp3;
		}
		
		return FMOD::Sound::release();
	}
};

#include <Geode/modify/System.hpp>
class $modify(FMOD::System) {
	FMOD_RESULT createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, FMOD::Sound **sound) {
		
		std::string str = std::string(name_or_data);
		if (!str.ends_with("mp3")) {
			log::debug("Probably not mp3, using FMOD: {}", name_or_data);
			return FMOD::System::createStream(name_or_data, mode, exinfo, sound);
		}
		
		unsigned int bufferLength;
		drmp3_int64  frames;
		drmp3_bool32 mp3Result;
		FMOD_CREATESOUNDEXINFO info;
		
		// GD doesn't use exinfo, should be safe? Idk i'm new here
		memset(&info, 0, sizeof(info));
		info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		auto mp3 = std::make_unique<drmp3>(); // thanks to gemini.... oh well.....
		
		log::debug("Creating Stream: {}", name_or_data);
		
		mp3Result = drmp3_init_file(mp3.get(), name_or_data, 0);
		if (!mp3Result) {
			log::warn("Could not load: {}", mp3Result);
			return FMOD::System::createStream(name_or_data, mode, exinfo, sound);
		}
		
		frames = drmp3_get_pcm_frame_count(mp3.get());
		
		log::debug("MP3 frames: {}", frames);
		
		bufferLength = frames * sizeof(drmp3_int16) * mp3->channels;
		
		info.numchannels       = mp3->channels;
		info.defaultfrequency  = mp3->sampleRate;
		info.format            = FMOD_SOUND_FORMAT_PCM16;
		info.length            = bufferLength;
		info.pcmreadcallback   = *readBuffer;
		info.pcmsetposcallback = *seekBuffer;
		info.decodebuffersize  = 8192;
		info.userdata          = mp3.release();
		
		log::debug("MP3: {}", (void* )&mp3);
		log::debug("Creating stream from raw");

		FMOD_RESULT result = FMOD::System::createStream("", mode | FMOD_OPENUSER, &info, sound);
		
		log::debug("FMOD_RESULT {}", (int)result);
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
