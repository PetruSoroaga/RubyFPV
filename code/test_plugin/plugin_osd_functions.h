// This are the functions that can be exported by a OSD plugin:

// Mandatory:

void init(void* pEngine);
char* getName();
char* getUID();
void render(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight);

// Optional:

int getVersion();
int getPluginSettingsCount();
char* getPluginSettingName(int settingIndex);
int getPluginSettingType(int settingIndex);
int getPluginSettingMinValue(int settingIndex);
int getPluginSettingMaxValue(int settingIndex);
int getPluginSettingDefaultValue(int settingIndex);
int getPluginSettingOptionsCount(int settingIndex);
char* getPluginSettingOptionName(int settingIndex, int optionIndex);
float getDefaultWidth();
float getDefaultHeight();

void onNewVehicle(u32 uVehicleId);
int requestTelemetryStreams();
void onTelemetryStreamData(u8* pData, int nDataLength, int nTelemetryType);
