// Fill out your copyright notice in the Description page of Project Settings.

#include "HuntMeIfYouCan.h"
#include "AssassinCharacter.h"
#include "UnrealNetwork.h"

AAssassinCharacter::AAssassinCharacter():
    bIsStab( false )
{
}

void AAssassinCharacter::BeginPlay()
{
    Super::BeginPlay();

    SetCurrentHuntSkill( EHuntSkillEnum::HSE_ConcealedItem );

    InitDagger();
}

void AAssassinCharacter::InitDagger()
{
    FActorSpawnParameters SpawnParameter;

    SpawnParameter.Owner = this;

    ConcealedItemActor = GetWorld()->SpawnActor<AActor>( ( nullptr == DaggerActor ) ? AActor::StaticClass() : DaggerActor, SpawnParameter );

    ConcealedItemActor->SetActorHiddenInGame( true );

    ConcealedItemActor->AttachRootComponentTo( GetMesh(), FName( "ConcealedItem" ), EAttachLocation::SnapToTarget, false );
}

void AAssassinCharacter::Tick( float DeltaTime )
{
    Super::Tick( DeltaTime );
}

void AAssassinCharacter::SetupPlayerInputComponent( class UInputComponent* InputComponent )
{
    Super::SetupPlayerInputComponent( InputComponent );

    InputComponent->BindAction( "Jump", IE_Pressed, this, &AAssassinCharacter::JumpPrepared );
    InputComponent->BindAction( "Jump", IE_Released, this, &AAssassinCharacter::JumpDeliver );

    InputComponent->BindAction( "UseSkill", IE_Pressed, this, &AAssassinCharacter::UseSkill );

    InputComponent->BindAction( "ConcealedItem", IE_Pressed, this, &AAssassinCharacter::SwitchConcealedItem );
    InputComponent->BindAction( "TargetItem", IE_Pressed, this, &AAssassinCharacter::SwitchTargetItem );
    InputComponent->BindAction( "Unique", IE_Pressed, this, &AAssassinCharacter::SwitchUnique );
}

void AAssassinCharacter::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
    Super::GetLifetimeReplicatedProps( OutLifetimeProps );

    DOREPLIFETIME( AAssassinCharacter, bIsStab );
    DOREPLIFETIME( AAssassinCharacter, CurrentStatus );
    DOREPLIFETIME( AAssassinCharacter, CurrentHuntSkill );
    DOREPLIFETIME( AAssassinCharacter, CurrentRunningSkill );
}

void AAssassinCharacter::UseSkill()
{
    if ( EStatusEnum::SE_Dead == CurrentStatus || EStatusEnum::SE_Expose == CurrentStatus )
    {
        return;
    }

    switch ( CurrentHuntSkill )
    {
    case EHuntSkillEnum::HSE_ConcealedItem:
        UseConcealedItem();
        break;
    case EHuntSkillEnum::HSE_TargetItem:
        UseTargetItem();
        break;
    case EHuntSkillEnum::HSE_Unique:
        UseUnique();
        break;
    default:
        break;
    }
}

void AAssassinCharacter::UseConcealedItem()
{
    if ( Role < ROLE_Authority )
    {
        ServerUseConcealedItem();
    }

    SetStabBegin();

    SetActorRotation( FRotator( 0.0f, Controller->GetControlRotation().Yaw, 0.0f ) );

    FCollisionQueryParams TraceParams = FCollisionQueryParams( FName( TEXT( "Trace" ) ), false, this );

    FHitResult Result;

    GetWorld()->LineTraceSingleByChannel( Result, GetActorLocation(),
                                          GetActorLocation() + GetViewRotation().RotateVector( FVector( 100.0f, 0.0f, 0.0f ) ), ECC_Pawn, TraceParams );

    AActor* Actor = Result.GetActor();

    if ( !Result.bBlockingHit || !Actor->IsA( ANormalCharacter::StaticClass() ) )
    {
        return;
    }

    if ( !Cast<ANormalCharacter>( Actor )->OnPlayerHit() )
    {
        GoIntoStatus( EStatusEnum::SE_Expose );
    }
}

void AAssassinCharacter::ServerUseConcealedItem_Implementation()
{
    UseConcealedItem();
}

bool AAssassinCharacter::ServerUseConcealedItem_Validate()
{
    return true;
}

void AAssassinCharacter::UseTargetItem()
{
}

void AAssassinCharacter::UseUnique()
{
}

void AAssassinCharacter::GoIntoStatus( EStatusEnum NewStatus )
{
    switch ( NewStatus )
    {
    case  EStatusEnum::SE_Expose:
        BeExpose();
        break;
    case EStatusEnum::SE_Crawling:
        BeCrawling();
    default:
        break;
    }

    SetCurrentStatus( NewStatus );
}

void AAssassinCharacter::SetStabBegin()
{
    SetStab( true );

    GetWorldTimerManager().ClearTimer( StabTimer );
    GetWorldTimerManager().SetTimer( StabTimer, this, &AAssassinCharacter::SetStabOver, 0.3f, false );

    ConcealedItemActor->SetActorHiddenInGame( false );
}

void AAssassinCharacter::SetStabOver()
{
    SetStab( false );
    ConcealedItemActor->SetActorHiddenInGame( true );
}

void AAssassinCharacter::SetStab( bool IsStab )
{
    bIsStab = IsStab;

    // If this next check succeeds, we are *not* the authority, meaning we are a network client.
    // In this case we also want to call the server function to tell it to change the bSomeBool property as well.
    if ( Role < ROLE_Authority )
    {
        ServerSetStab( IsStab );
    }
}

void AAssassinCharacter::ServerSetStab_Implementation( bool IsStab )
{
    // This function is only called on the server (where Role == ROLE_Authority), called over the network by clients.
    // We need to call SetSomeBool() to actually change the value of the bool now!
    // Inside that function, Role == ROLE_Authority, so it won't try to call ServerSetSomeBool() again.
    SetStab( IsStab );
    ConcealedItemActor->SetActorHiddenInGame( !IsStab );
}

bool AAssassinCharacter::ServerSetStab_Validate( bool IsStab )
{
    return true;
}

void AAssassinCharacter::SetCurrentStatus( EStatusEnum Status )
{
    CurrentStatus = Status;

    if ( Role < ROLE_Authority )
    {
        ServerSetCurrentStatus( Status );
    }
}

void AAssassinCharacter::ServerSetCurrentStatus_Implementation( EStatusEnum Status )
{
    SetCurrentStatus( Status );
}

bool AAssassinCharacter::ServerSetCurrentStatus_Validate( EStatusEnum Status )
{
    return true;
}

void AAssassinCharacter::SetCurrentHuntSkill( EHuntSkillEnum Skill )
{
    CurrentHuntSkill = Skill;

    if ( Role < ROLE_Authority )
    {
        ServerSetCurrentHuntSkill( Skill );
    }
}

void AAssassinCharacter::ServerSetCurrentHuntSkill_Implementation( EHuntSkillEnum Skill )
{
    SetCurrentHuntSkill( CurrentHuntSkill );
}

bool AAssassinCharacter::ServerSetCurrentHuntSkill_Validate( EHuntSkillEnum Skill )
{
    return true;
}

void AAssassinCharacter::SetCurrentRunningSkill( ERunningSkillEnum Skill )
{
    CurrentRunningSkill = Skill;

    if ( Role < ROLE_Authority )
    {
        ServerSetCurrentRunningSkill( Skill );
    }
}

void AAssassinCharacter::ServerSetCurrentRunningSkill_Implementation( ERunningSkillEnum Skill )
{
    SetCurrentRunningSkill( Skill );
}

bool AAssassinCharacter::ServerSetCurrentRunningSkill_Validate( ERunningSkillEnum Skill )
{
    return true;
}

void AAssassinCharacter::SwitchConcealedItem()
{
    SetCurrentHuntSkill( EHuntSkillEnum::HSE_ConcealedItem );
}

void AAssassinCharacter::SwitchTargetItem()
{
    SetCurrentHuntSkill( EHuntSkillEnum::HSE_TargetItem );
}

void AAssassinCharacter::SwitchUnique()
{
    SetCurrentHuntSkill( EHuntSkillEnum::HSE_Unique );
}

void AAssassinCharacter::Exposed()
{
    auto MeshMaterialInstances = GetMeshMaterialInstances();

    MeshMaterialInstances[EMaterialInstanceIDEnum::MII_Head]->SetTextureParameterValue(
        FName( "ManHeadTextureParameter" ), GetMeshTexture( EMaterialInstanceIDEnum::MII_Head ) );

    MeshMaterialInstances[EMaterialInstanceIDEnum::MII_Hand]->SetTextureParameterValue(
        FName( "ManHandTextureParameter" ), GetMeshTexture( EMaterialInstanceIDEnum::MII_Hand ) );

    MeshMaterialInstances[EMaterialInstanceIDEnum::MII_Foot]->SetTextureParameterValue(
        FName( "ManFootTextureParameter" ), GetMeshTexture( EMaterialInstanceIDEnum::MII_Foot ) );

    MeshMaterialInstances[EMaterialInstanceIDEnum::MII_Body]->SetTextureParameterValue(
        FName( "ManBodyTextureParameter" ), GetMeshTexture( EMaterialInstanceIDEnum::MII_Body ) );

    GetWorldTimerManager().ClearTimer( ExposeTimer );
    GetWorldTimerManager().SetTimer( ExposeTimer, this, &AAssassinCharacter::GoCrawling, 30.0f, false );
}

void AAssassinCharacter::BeExpose_Implementation()
{
    Exposed();
}

void AAssassinCharacter::BeCrawling()
{
    GetWorldTimerManager().ClearTimer( CrawlingTimer );
    GetWorldTimerManager().SetTimer( CrawlingTimer, this, &AAssassinCharacter::GoMasquerade, 5.0f, false );

    GEngine->AddOnScreenDebugMessage( -1, 3.0, FColor::Yellow, TEXT( "BeCrawling" ) );
}

void AAssassinCharacter::GoCrawling()
{
    if ( EStatusEnum::SE_Dead == CurrentStatus )
    {
        return;
    }

    GoIntoStatus( EStatusEnum::SE_Crawling );

    SetActorHiddenInGame( true );
    SetActorEnableCollision( false );
}

bool AAssassinCharacter::OnPlayerHit()
{
    ANormalCharacter::OnPlayerHit();
    GoIntoStatus( EStatusEnum::SE_Crawling );
    return true;
}

void AAssassinCharacter::JumpPrepared()
{
    if ( EStatusEnum::SE_Dead == CurrentStatus )
    {
        return;
    }
    Jump();
}

void AAssassinCharacter::JumpDeliver()
{
    StopJumping();
}

UTexture2D * AAssassinCharacter::GetMeshTexture( int32 ID )
{
    UTexture2D* Texture = nullptr;
    switch ( ID )
    {
    case EMaterialInstanceIDEnum::MII_Head:
        Texture = HeadMeshTexture;
        break;
    case	EMaterialInstanceIDEnum::MII_Hand:
        Texture = HandMeshTexture;
        break;
    case	EMaterialInstanceIDEnum::MII_Foot:
        Texture = FootMeshTexture;
        break;
    case	EMaterialInstanceIDEnum::MII_Body:
        Texture = BodyMeshTexture;
        break;
    default:
        break;
    }
    return Texture;
}

void AAssassinCharacter::GoMasquerade()
{
    RandomMeshTexture();

    SetActorEnableCollision( true );
    SetActorHiddenInGame( false );
}