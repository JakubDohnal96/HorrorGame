// Copyright Epic Games, Inc. All Rights Reserved.

#include "HorrorGameGameMode.h"
#include "HorrorGameCharacter.h"
#include "UObject/ConstructorHelpers.h"

AHorrorGameGameMode::AHorrorGameGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
