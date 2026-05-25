#include <Geode/Geode.hpp>

#include <Geode/fmod/fmod.hpp>
#include <Geode/fmod/fmod_common.h>

using namespace geode::prelude;

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

// class Mp3Read {
// public:
// 	drmp3 mp3;
// 	Mp3Read create(drmp3 mp3) {
// 		Mp3Read x;
// 		x.mp3 = mp3;
// 		return x;
// 	};
// 	static FMOD_RESULT F_CALLBACK readBuffer(FMOD_SOUND* sound, void* data, unsigned int datalen) {
// 		drmp3_read_pcm_frames_s16(&this->mp3, datalen, (drmp3_int16* )data);
// 		return FMOD_OK;
// 	};
// };

FMOD_RESULT F_CALLBACK readBuffer(FMOD_SOUND* sound, void* data, unsigned int datalen) {
	drmp3 mp3;
	void* pointer;
	FMOD_Sound_GetUserData(sound, &pointer);
	mp3 = *(drmp3* )pointer;
	log::debug("MP3: {}, {}, {}, {}, {}", (void*)&mp3, (void*)pointer, mp3.pcmFramesConsumedInMP3Frame, mp3.channels, mp3.sampleRate);
	log::debug("At {} writing {}", (void*)data, datalen);
	drmp3_read_pcm_frames_s16(&mp3, datalen/8, (drmp3_int16* )data);
	log::debug("Done");
	return FMOD_OK;
};

#include <Geode/modify/System.hpp>
class $modify(FMOD::System) {
	FMOD_RESULT createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, FMOD::Sound **sound) {
		drmp3         mp3;
		unsigned int  bufferLength;
		drmp3_int64   frames = 0;
		// drmp3_int16*  buffer;
		drmp3_uint64  framesRead = 0;
		drmp3_bool32  mp3Result = 0;
		unsigned int  modeFlags;
		// Mp3Read       bufferReader;
		// GD doesn't use exinfo, should be safe? Idk i'm new here
		FMOD_CREATESOUNDEXINFO info;
		memset(&info, 0, sizeof(info));
		info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		memset(&mp3, 0, sizeof(mp3));

		
		log::debug("Creating Stream: {}", name_or_data);
		
		mp3Result = drmp3_init_file(&mp3, name_or_data, 0);
		if (!mp3Result) {
			log::debug("Could not load: {}", mp3Result);
			return FMOD::System::createStream(name_or_data, mode, exinfo, sound);
		}
		
		frames = drmp3_get_pcm_frame_count(&mp3);
		log::debug("MP3 frames: {}", frames);
		
		bufferLength = frames * sizeof(drmp3_int16) * mp3.channels;
		// buffer = new drmp3_int16[bufferLength];
		
		// bufferReader.mp3 = mp3;
		
		
		info.numchannels      = 2;
		info.defaultfrequency = mp3.sampleRate;
		info.format           = FMOD_SOUND_FORMAT_PCM16;
		info.length           = bufferLength;
		info.pcmreadcallback  = *readBuffer;
		info.decodebuffersize = 5120;
		info.userdata         = &mp3;
		// FMOD_Sound_SetUserData(*sound, (void* )&mp3);
		
		// framesRead = drmp3_read_pcm_frames_s16(&mp3, frames, buffer);
		// log::debug("framesRead: {}", framesRead);

		log::debug("Creating stream from raw");
		
		log::debug("MP3: {}", (void* )&mp3);

		modeFlags = FMOD_LOWMEM | FMOD_LOOP_NORMAL | FMOD_2D | FMOD_ACCURATETIME | FMOD_NONBLOCKING | FMOD_OPENUSER;
		FMOD_RESULT result = FMOD::System::createStream("", modeFlags, &info, sound);
		log::debug("FMOD_RESULT {}", (int)result);
		// free(buffer);
		return result;
	}
};

#include <Geode/binding/GameManager.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
class $modify(FMODAudioEngine) {
	void setupAudioEngine() {
		// FMODAudioEngine::setupAudioEngine(); // this does not work
		
		FMOD::System*      system;
		unsigned int       FMODVersion;
		unsigned int       fileBufferSize;
		FMOD_TIMEUNIT      fileBufferSizeType;
		unsigned int       bufferLength;
		int                numBuffers;
		int                samplerate;
		FMOD_SPEAKERMODE   speakerMode;
		int                numRawSpeakers;
		GameManager*       gameManager;
		
		FMOD_ADVANCEDSETTINGS settings;
		memset(&settings, 0, sizeof(FMOD_ADVANCEDSETTINGS));
		settings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);

		settings.resamplerMethod = FMOD_DSP_RESAMPLER_SPLINE;

		
		this->m_lastResult = FMOD::System_Create(&system, 0x20231);
		this->m_system = system;
		
		system->getVersion(&FMODVersion);
		FMOD::Debug_Initialize(0);
		system->getStreamBufferSize(&fileBufferSize, &fileBufferSizeType);

		
		system->getDSPBufferSize(&bufferLength, &numBuffers);
		system->getSoftwareFormat(&samplerate, &speakerMode, &numRawSpeakers);
		
		gameManager = GameManager::get();
		if (gameManager->getGameVariable(GameVar::IncreaseAudioBuffer)) {
			bufferLength = 512;
		}
		if (gameManager->getGameVariable(GameVar::ReduceAudioQuality)) {
			this->m_reducedQuality = true;
			samplerate = 24000;
		}
		
		if (Mod::get()->getSettingValue<bool>("double-sr")) { // Doubling sr kinda helps
			bufferLength *= 2;
			samplerate *= 2;
		}
		
		system->setDSPBufferSize(bufferLength, numBuffers);
		system->setSoftwareFormat(samplerate, speakerMode, numRawSpeakers); 
		this->m_sampleRate = samplerate;
		
		
		system->setAdvancedSettings(&settings);
		
		this->m_lastResult = system->init(128, 0, 0); // todo third argument
		
		system->createChannelGroup(0, &this->m_backgroundMusicChannel);
		this->m_backgroundMusicChannel->setVolumeRamp(false);
		this->m_backgroundMusicChannel->getDSP(-1, &this->m_mainDSP);
		this->m_mainDSP->setMeteringEnabled(false,true);
		
		system->createChannelGroup(0, &this->m_globalChannel);
		system->createChannelGroup(0, &this->m_reverbChannel);
		this->m_globalChannel->addGroup(this->m_reverbChannel, true, 0);
		
		FMOD::DSP *dsp;
		system->createDSPByType(FMOD_DSP_TYPE_LIMITER,&dsp);
		dsp->setParameterFloat(1, 0.0f);
		dsp->setParameterBool(3, true);
		this->m_globalChannel->addDSP(1, dsp);
		
		system->createDSPByType(FMOD_DSP_TYPE_LIMITER,&dsp);
		dsp->setParameterFloat(1, 0.0f);
		dsp->setParameterBool(3, true);
		this->m_backgroundMusicChannel->addDSP(1, dsp);
		
		FMOD::DSP *reverbDSP;
		system->createDSPByType(FMOD_DSP_TYPE_SFXREVERB, &reverbDSP);
		this->m_reverbChannel->addDSP(0, reverbDSP);
		
		this->updateReverb(this->m_reverbPreset, true);
		
		this->m_globalChannel->getDSP(-1, &this->m_globalChannelDSP);
		this->m_globalChannelDSP->setMeteringEnabled(false, true);
	}
};