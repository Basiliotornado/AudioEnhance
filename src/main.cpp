#include <Geode/Geode.hpp>

#include <Geode/fmod/fmod.hpp>
// #include <Geode/fmod/fmod_common.h>

using namespace geode::prelude;

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include <Geode/modify/System.hpp>
class $modify(FMOD::System) {
	FMOD_RESULT createStream(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, FMOD::Sound **sound) {
		log::debug("Creating Stream: {}", name_or_data);
		
		drmp3 mp3;
		drmp3_bool32 x = drmp3_init_file(&mp3, name_or_data, 0);
		log::debug("{}", x);
		if (!x) {
			log::debug("Could not load: {}", name_or_data, x);
			return FMOD::System::createStream(name_or_data, mode, exinfo, sound);
		}
		
		drmp3_int64 frames = drmp3_get_pcm_frame_count(&mp3);
		log::debug("MP3 frames: {}", frames);
		
		unsigned int buffer_length = frames * sizeof(drmp3_int16) * mp3.channels;
		drmp3_int16 *buffer = new drmp3_int16[buffer_length];
		
		drmp3_uint64 framesRead = drmp3_read_pcm_frames_s16(&mp3, frames, buffer);
		log::debug("framesRead: {}", framesRead);

		// GD doesn't use exinfo, should be safe? Idk i'm new here
		FMOD_CREATESOUNDEXINFO info;
		memset(&info, 0, sizeof(info));
		info.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		
		info.numchannels = 2;
		info.defaultfrequency = mp3.sampleRate;
		info.format = FMOD_SOUND_FORMAT_PCM16;
		info.length = buffer_length;
		
		log::debug("Creating stream from raw");
		
		FMOD_RESULT result = FMOD::System::createStream((const char*)buffer, mode | FMOD_OPENMEMORY | FMOD_OPENRAW, &info, sound);
		log::debug("FMOD_RESULT {}", (int)result);
		return result;
	}
	
	// FMOD_RESULT createSound(const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, FMOD::Sound **sound) {
	// 	// std::string path = name_or_data;
	// 	log::debug("Creating Sound {}", name_or_data);
	// 	return (FMOD_RESULT)0;
	// }	
	// FMOD_RESULT playSound(FMOD::Sound *sound, FMOD::ChannelGroup *channelgroup, bool paused, FMOD::Channel **channel) {
	// 	return (FMOD_RESULT)0;
	// }
};

#include <Geode/binding/GameManager.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
class $modify(FMODAudioEngine) {
	void setupAudioEngine() {
		// FMODAudioEngine::setupAudioEngine(); // this does not work
		
		FMOD::System       *system;
		unsigned int        FMODVersion;
		unsigned int        filebuffersize;
		FMOD_TIMEUNIT       filebuffersizetype;
		unsigned int        bufferlength;
		int                 numbuffers;
		int                 samplerate;
		FMOD_SPEAKERMODE    speakermode;
		int                 numrawspeakers;
		GameManager        *gameManager;
		
		
		FMOD_ADVANCEDSETTINGS  settings;
		memset(&settings, 0, sizeof(FMOD_ADVANCEDSETTINGS));
		settings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);

		settings.resamplerMethod = FMOD_DSP_RESAMPLER_SPLINE;

		
		this->m_lastResult = FMOD::System_Create(&system, 0x20231);
		this->m_system = system;
		
		system->getVersion(&FMODVersion);
		FMOD::Debug_Initialize(0);
		system->getStreamBufferSize(&filebuffersize, &filebuffersizetype);

		
		system->getDSPBufferSize(&bufferlength, &numbuffers);
		system->getSoftwareFormat(&samplerate, &speakermode, &numrawspeakers);
		
		gameManager = GameManager::get();
		if (gameManager->getGameVariable(GameVar::IncreaseAudioBuffer)) {
			bufferlength = 512;
		}
		if (gameManager->getGameVariable(GameVar::ReduceAudioQuality)) {
			this->m_reducedQuality = true;
			samplerate = 24000;
		}
		
		if (Mod::get()->getSettingValue<bool>("double-sr")) { // Doubling sr kinda helps
			bufferlength *= 2;
			samplerate *= 2;
		}
		
		system->setDSPBufferSize(bufferlength, numbuffers);
		system->setSoftwareFormat(samplerate, speakermode, numrawspeakers); 
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
		dsp->setParameterFloat(1, 0.0);
		dsp->setParameterBool(3, true);
		this->m_globalChannel->addDSP(1, dsp);
		
		system->createDSPByType(FMOD_DSP_TYPE_LIMITER,&dsp);
		dsp->setParameterFloat(1, 0.0);
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