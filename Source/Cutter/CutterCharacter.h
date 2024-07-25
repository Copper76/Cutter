// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "CutterCharacter.generated.h"

class UProceduralMeshComponent;

UCLASS(config=Game)
class ACutterCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

	/** Cutting plane */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* CuttingPlane;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Cut Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* CutAction;

	/** Toggle Cut Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* ToggleCutAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Tiemline, meta = (AllowPrivateAccess = "true"))
	UCurveFloat* m_CameraMoveCurve;

public:
	ACutterCharacter();
	
protected:

	void CutterJump(const FInputActionValue& Value);

	void CutterStopJumping(const FInputActionValue& Value);

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void Cut(const FInputActionValue& Value);
			
	void ToggleCut(const FInputActionValue& Value);

	UProceduralMeshComponent* StaticToProceduralMesh(UStaticMeshComponent* staticMeshComponent, AActor* owner);

	UProceduralMeshComponent* AddSimpleCollision(UProceduralMeshComponent* proceduralMeshComponent);

	UProceduralMeshComponent* ActivatePhysics(UProceduralMeshComponent* proceduralMeshComponent, UStaticMeshComponent* staticMeshComponent);

	void Tick(float deltaTime) override;

private:
	bool m_IsCuttingMode = false;

	float m_CameraMoveTime = 0.0f;

	float m_CameraTargetArmLength;

	float m_CameraStartArmLength;

	FVector m_CameraTargetOffset;

	FVector m_CameraStartOffset;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

