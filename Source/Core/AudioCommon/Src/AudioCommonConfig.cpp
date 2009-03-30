#include "AudioCommon.h"

AudioCommonConfig ac_Config;

// Load from given file
void AudioCommonConfig::Load(IniFile &file) {
	file.Get("Config", "EnableDTKMusic", &m_EnableDTKMusic, true);
	file.Get("Config", "EnableThrottle", &m_EnableThrottle, true);
#ifdef _WIN32
	file.Get("Config", "Backend", &sBackend, "DSound");
#else
	file.Get("Config", "Backend", &sBackend, "AOSound");
#endif
}

// Set the values for the file
void AudioCommonConfig::Set(IniFile &file) {
	file.Set("Config", "EnableDTKMusic", m_EnableDTKMusic);
	file.Set("Config", "EnableThrottle", m_EnableThrottle);
	file.Set("Config", "Backend", sBackend.c_str());
}

// Update according to the values (stream/mixer)
void AudioCommonConfig::Update() {
	if (soundStream) {
		soundStream->GetMixer()->SetThrottle(m_EnableThrottle);
		soundStream->GetMixer()->SetDTKMusic(m_EnableDTKMusic);
	}
}
