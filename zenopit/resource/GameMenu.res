"GameMenu"
{
	"1"
	{
		"label" "#GameUI_GameMenu_ResumeGame"
		"command" "ResumeGame"
		"InGameOrder" "10"
		"OnlyInGame" "1"
		"center"		"1"
	}
	"2"	
	{
		"label" "#GameUI_GameMenu_NewGame"
		"command" "engine map zp_test"
		"InGameOrder" "20"
		"notmulti" "1"
		"center"		"1"
	}
	"3"	[$WIN32]
	{
		"label" "#GameUI_GameMenu_BonusMaps"
		"command" "OpenBonusMapsDialog"
		"InGameOrder" "30"
		"notmulti" "1"
	}
	"6"
	{
		"label" "#GameUI_GameMenu_LoadGame"
		"command" "OpenLoadGameDialog"
		"InGameOrder" "40"
		"notmulti" "1"
		"center"		"1"
	}
	//"7"
	//{
	//	"label" "#GameUI_GameMenu_SaveGame"
	//	"command" "OpenSaveGameDialog"
	//	"InGameOrder" "20"
	//	"notmulti" "1"
	//	"OnlyInGame" "1"
	//}
	//"8"
	//{
	//	"label"	"#GameUI_LoadCommentary"
	//	"command" "OpenLoadSingleplayerCommentaryDialog"
	//	"InGameOrder" "50"
	//}
	//"9"
	//{
	//	"label" "#GameUI_GameMenu_Achievements"
	//	"command" "OpenAchievementsDialog"
	//	"InGameOrder" "60"
	//}
	"10"
	{
		"label" "#GameUI_Controller"
		"command" "OpenControllerDialog"
		"InGameOrder" "70"
		"ConsoleOnly" "1"
	}
	"11"
	{
		"label" "#GameUI_GameMenu_Options"
		"command" "OpenOptionsDialog"
		"InGameOrder" "80"
	}
	"12"
	{
		"label" "#GameUI_GameMenu_Quit"
		"command" "Quit"
		"InGameOrder" "90"
	}
}

