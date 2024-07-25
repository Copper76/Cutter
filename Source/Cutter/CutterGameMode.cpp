// Copyright Epic Games, Inc. All Rights Reserved.

#include "CutterGameMode.h"
#include "CutterCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACutterGameMode::ACutterGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
