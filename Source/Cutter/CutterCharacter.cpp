// Copyright Epic Games, Inc. All Rights Reserved.

#include "CutterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"

//////////////////////////////////////////////////////////////////////////
// ACutterCharacter

ACutterCharacter::ACutterCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	CuttingPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cutter"));
	CuttingPlane->SetupAttachment(RootComponent);
	CuttingPlane->AddLocalOffset(FVector(270.0f, 0.0f, 40.0f));
	CuttingPlane->SetActive(false);
	CuttingPlane->SetVisibility(false);
	CuttingPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ACutterCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ACutterCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACutterCharacter::CutterJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACutterCharacter::CutterStopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACutterCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACutterCharacter::Look);

		//Cutting
		EnhancedInputComponent->BindAction(CutAction, ETriggerEvent::Triggered, this, &ACutterCharacter::Cut);

		//Toggle cut
		EnhancedInputComponent->BindAction(ToggleCutAction, ETriggerEvent::Triggered, this, &ACutterCharacter::ToggleCut);

	}
}

void ACutterCharacter::CutterJump(const FInputActionValue& Value)
{
	if (m_IsCuttingMode) return;

	Jump();
}

void ACutterCharacter::CutterStopJumping(const FInputActionValue& Value)
{
	if (m_IsCuttingMode) return;

	StopJumping();
}

void ACutterCharacter::Move(const FInputActionValue& Value)
{
	if (m_IsCuttingMode) return;

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ACutterCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (m_IsCuttingMode)
	{
		CuttingPlane->SetRelativeRotation(FRotator(0.0f, 0.0f, LookAxisVector.X) + CuttingPlane->GetRelativeRotation());
	}
	else
	{
		if (Controller != nullptr)
		{
			// add yaw and pitch input to controller
			AddControllerYawInput(LookAxisVector.X);
			AddControllerPitchInput(LookAxisVector.Y);
		}
	}
}

void ACutterCharacter::Cut(const FInputActionValue& Value)
{
	if (!m_IsCuttingMode) return;

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FVector bbox = CuttingPlane->GetStaticMesh()->GetBoundingBox().GetExtent();
	bbox.Z = 1.0f;

	if (GetWorld()->OverlapMultiByObjectType(OverlapResults, CuttingPlane->GetComponentLocation(), FQuat(CuttingPlane->GetComponentRotation()), FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeBox(bbox * CuttingPlane->GetRelativeScale3D()), QueryParams))
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			UProceduralMeshComponent* proceduralMesh = Cast<UProceduralMeshComponent>(Result.GetComponent());
			if (!proceduralMesh)
			{
				if (UStaticMeshComponent* staticMeshComponent = Cast<UStaticMeshComponent>(Result.GetComponent()))
				{
					if (staticMeshComponent->Mobility == EComponentMobility::Static) return;

					proceduralMesh = StaticToProceduralMesh(staticMeshComponent, Result.GetActor());
				}
			}
			if (proceduralMesh == nullptr || proceduralMesh->Mobility == EComponentMobility::Static) return;

			UProceduralMeshComponent* OtherHalf;
			UKismetProceduralMeshLibrary::SliceProceduralMesh(proceduralMesh, CuttingPlane->GetComponentLocation(), CuttingPlane->GetUpVector(), true, OtherHalf, EProcMeshSliceCapOption::CreateNewSectionForCap, proceduralMesh->GetMaterial(0));

			proceduralMesh->AddImpulse(CuttingPlane->GetUpVector() * 500.0f, FName(), true);

			//AActor* NewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), OtherHalf->GetComponentLocation(), OtherHalf->GetComponentRotation());
			//OtherHalf->Rename(*OtherHalf->GetName(), NewActor);//In order to have multiple actors
			//NewActor->SetRootComponent(OtherHalf);

			OtherHalf->SetSimulatePhysics(true);
			OtherHalf->SetGenerateOverlapEvents(false);
			OtherHalf->AddImpulse(CuttingPlane->GetUpVector() * 500.0f * -1.0f, FName(), true);
		}
	}
}

void ACutterCharacter::ToggleCut(const FInputActionValue& Value)
{
	m_IsCuttingMode = !m_IsCuttingMode;

	APlayerController* controller = Cast<APlayerController>(GetController());

	if (m_IsCuttingMode)
	{
		controller->SetControlRotation(FRotator(-10.0f, GetActorRotation().Yaw, 0.0f));
		CameraBoom->TargetArmLength = 100.0f;
		CameraBoom->SetRelativeLocation(FVector(0.0f, 30.0f, 80.0f));
		CuttingPlane->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		CuttingPlane->SetActive(true);
		CuttingPlane->SetVisibility(true);
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.1f);
	}
	else
	{
		CuttingPlane->SetActive(false);
		CuttingPlane->SetVisibility(false);
		CameraBoom->TargetArmLength = 400.0f;
		CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
	}
}

UProceduralMeshComponent* ACutterCharacter::StaticToProceduralMesh(UStaticMeshComponent* staticMeshComponent, AActor* owner)
{
	//Create procedural mesh component
	UProceduralMeshComponent* proceduralMeshComponent = NewObject<UProceduralMeshComponent>(owner, UProceduralMeshComponent::StaticClass());

	if (proceduralMeshComponent == nullptr) return nullptr;

	proceduralMeshComponent->RegisterComponent();

	//Replace root component if necessary
	if (owner->GetRootComponent() == staticMeshComponent)
	{
		owner->SetRootComponent(proceduralMeshComponent);
	}
	else
	{
		proceduralMeshComponent->AttachToComponent(owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}

	// Copy transform
	proceduralMeshComponent->SetRelativeTransform(staticMeshComponent->GetRelativeTransform());

	// Copy materials
	int32 NumMaterials = staticMeshComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		proceduralMeshComponent->SetMaterial(MaterialIndex, staticMeshComponent->GetMaterial(MaterialIndex));
	}

	UStaticMesh* staticMesh = staticMeshComponent->GetStaticMesh();

	if (staticMesh)
	{
		for (int32 LODIndex = 0; LODIndex < staticMesh->GetNumLODs(); ++LODIndex)
		{
			FStaticMeshLODResources& LODResource = staticMesh->GetRenderData()->LODResources[LODIndex];

			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FProcMeshTangent> Tangents;

			// Extract mesh data from LODResource
			for (FStaticMeshSection& Section : LODResource.Sections)
			{
				TArray<uint32> Indices;
				LODResource.IndexBuffer.GetCopy(Indices);
				uint32 maxIndex = LODResource.VertexBuffers.PositionVertexBuffer.GetNumVertices();

				for (uint32 i = 0 ; i < maxIndex; ++i)
				{
					Vertices.Add(FVector(LODResource.VertexBuffers.PositionVertexBuffer.VertexPosition(i)));
					Normals.Add(FVector(LODResource.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(i)));
					UVs.Add(FVector2D(LODResource.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(i, 0)));
				}

				for (uint32 Index : Indices)
				{

					Triangles.Add(Index);
				}
			}

			// Create section on procedural mesh
			proceduralMeshComponent->CreateMeshSection(LODIndex, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, LODIndex==0);
		}
	}

	proceduralMeshComponent = AddSimpleCollision(proceduralMeshComponent);

	proceduralMeshComponent = ActivatePhysics(proceduralMeshComponent, staticMeshComponent);

	staticMeshComponent->DestroyComponent();

	return proceduralMeshComponent;
}

UProceduralMeshComponent* ACutterCharacter::AddSimpleCollision(UProceduralMeshComponent* proceduralMeshComponent)
{
	proceduralMeshComponent->bUseComplexAsSimpleCollision = false;
	UBodySetup* BodySetup = proceduralMeshComponent->GetBodySetup();
	BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

	// Clear any existing collision
	BodySetup->RemoveSimpleCollision();

	// Extract vertices from the procedural mesh
	TArray<FVector> Vertices;
	for (FProcMeshVertex vert : proceduralMeshComponent->GetProcMeshSection(0)->ProcVertexBuffer)
	{
		Vertices.Add(vert.Position);
	}

	// Create a convex hull from the vertices
	FKConvexElem ConvexElem;
	ConvexElem.VertexData = Vertices;
	ConvexElem.UpdateElemBox();

	BodySetup->AggGeom.ConvexElems.Add(ConvexElem);

	// Regenerate collision
	proceduralMeshComponent->RecreatePhysicsState();

	return proceduralMeshComponent;
}

UProceduralMeshComponent* ACutterCharacter::ActivatePhysics(UProceduralMeshComponent* proceduralMeshComponent, UStaticMeshComponent* staticMeshComponent)
{
	// Copy physics properties
	proceduralMeshComponent->SetMassOverrideInKg(FName(), staticMeshComponent->GetMass());
	//proceduralMeshComponent->CalculateMass();
	proceduralMeshComponent->SetEnableGravity(staticMeshComponent->IsGravityEnabled());
	proceduralMeshComponent->SetCollisionProfileName(staticMeshComponent->GetCollisionProfileName());
	proceduralMeshComponent->SetCollisionEnabled(staticMeshComponent->GetCollisionEnabled());
	proceduralMeshComponent->SetCollisionResponseToChannels(staticMeshComponent->GetCollisionResponseToChannels());
	proceduralMeshComponent->SetNotifyRigidBodyCollision(staticMeshComponent->GetBodyInstance()->bNotifyRigidBodyCollision);
	proceduralMeshComponent->SetGenerateOverlapEvents(false);

	proceduralMeshComponent->bUseAsyncCooking = true;

	proceduralMeshComponent->SetSimulatePhysics(staticMeshComponent->IsSimulatingPhysics());

	proceduralMeshComponent->SetPhysicsLinearVelocity(staticMeshComponent->GetPhysicsLinearVelocity());
	proceduralMeshComponent->SetPhysicsAngularVelocityInRadians(staticMeshComponent->GetPhysicsAngularVelocityInRadians());

	return proceduralMeshComponent;
}
