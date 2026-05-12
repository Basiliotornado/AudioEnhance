#include <Geode/Geode.hpp>

#include <Geode/fmod/fmod.hpp>
// #include <Geode/fmod/fmod_common.h>

using namespace geode::prelude;

#include <Geode/binding/GameManager.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
class $modify(FMODAudioEngine) {
	void setupAudioEngine() {
		// FMODAudioEngine::setupAudioEngine(); // so broken if this runs, god help us
		
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
		FMOD::Debug_Initialize(0); // ok bro
		system->getStreamBufferSize(&filebuffersize, &filebuffersizetype);
		// todo GameManager calls
		
		
		system->getDSPBufferSize(&bufferlength, &numbuffers);
		system->getSoftwareFormat(&samplerate, &speakermode, &numrawspeakers);
		// todo set samplerate to game setting (44100/22050)
		
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
		
		
		system->setAdvancedSettings(&settings); // This is where the magic happens?
		
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