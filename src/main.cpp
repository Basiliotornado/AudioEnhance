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