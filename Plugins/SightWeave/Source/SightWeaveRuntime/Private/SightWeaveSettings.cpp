#include "SightWeaveSettings.h"

USightWeaveSettings::USightWeaveSettings()
	: DefaultFloorId(FName(TEXT("Default")))
{
}

FName USightWeaveSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

FName USightWeaveSettings::GetSectionName() const
{
	return FName(TEXT("SightWeave"));
}
