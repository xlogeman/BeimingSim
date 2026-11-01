// Fill out your copyright notice in the Description page of Project Settings.
#include "WeatherLib.h"
#include "Materials/MaterialParameterCollection.h"
#include "Blueprint/UserWidget.h"

AExponentialHeightFog* UWeatherLib::weather_fog_ = nullptr;

UMaterialParameterCollectionInstance* UWeatherLib::getWeatherMaterialCollectionInstance(UWorld* World)
{
    //UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (World) {
        UMaterialParameterCollection* WeatherParameterCollection = Cast<UMaterialParameterCollection>(StaticLoadObject(UMaterialParameterCollection::StaticClass(), NULL, getWeatherParamsObjectPath()));

        //UWorld* World = GetWorld();
        if (WeatherParameterCollection) {
            UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(WeatherParameterCollection);
            if (Instance) {
                return Instance;
            }
            else {
                UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get WeatherParameterCollectionInstance1!"));
            }
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get WeatherParameterCollection1!"));
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get World!"));
    }

    return NULL;
}
void UWeatherLib::initWeather(UWorld* World, TArray<AActor*> ActorsToAttachTo)
{
    //UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (World) {
        UClass* WeatherActorClass = getWeatherActorPath().TryLoadClass<AActor>();
        if (WeatherActorClass) {
            for (int32 i = 0; i < ActorsToAttachTo.Num(); i++) {
                const FVector Location = ActorsToAttachTo[i]->GetActorLocation();
                const FRotator Rotation = ActorsToAttachTo[i]->GetActorRotation();
                FActorSpawnParameters WeatherActorSpawnInfo;
                WeatherActorSpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                AActor* SpawnedWeatherActor = World->SpawnActor(WeatherActorClass, &Location, &Rotation, WeatherActorSpawnInfo);

                SpawnedWeatherActor->AttachToActor(ActorsToAttachTo[i], FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true));
            }
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI got invalid weather actor class!"));
        }
        // BEIMING patch: 原版在此 spawn F10 天气菜单的 MenuActor，该功能已由
        // 项目内 O 键仿真配置面板取代，不再生成
    }

    //showWeatherMenu(WorldContextObject);
}
void UWeatherLib::setWeatherParamScalar(UWorld* World, EWeatherParamScalar Param, float Amount)
{
    UMaterialParameterCollectionInstance* WeatherMaterialCollectionInstance = UWeatherLib::getWeatherMaterialCollectionInstance(World);
    if (WeatherMaterialCollectionInstance) {
        FName ParamName = GetWeatherParamScalarName(Param);
        if (ParamName == TEXT("")) {
            UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI got invalid paramname!"));
        }
        WeatherMaterialCollectionInstance->SetScalarParameterValue(ParamName, Amount);

        // if weather is not enabled, dont allow any weather values to be set
        // must be called after SetScalarParam, because WeatherEnabled is a scalar param
        // and must be set to true or false before this.
        // WeatherEnabled will always be false
        // NOTE: weather enabled must be set first, before other params for this to work
        if (!getIsWeatherEnabled(World)) {
            WeatherMaterialCollectionInstance->SetScalarParameterValue(ParamName, 0.0f);
        }

        if (weather_fog_) {
            weather_fog_->GetRootComponent()->SetVisibility(true);
        }
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get MaterialCollectionInstance!"));
    }
}
float UWeatherLib::getWeatherParamScalar(UWorld* World, EWeatherParamScalar Param)
{
    UMaterialParameterCollectionInstance* WeatherMaterialCollectionInstance = UWeatherLib::getWeatherMaterialCollectionInstance(World);
    if (WeatherMaterialCollectionInstance) {
        FName ParamName = GetWeatherParamScalarName(Param);
        if (ParamName == TEXT("")) {
            UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI got invalid paramname!"));
        }
        float Amount;
        WeatherMaterialCollectionInstance->GetScalarParameterValue(ParamName, Amount); //SetScalarParameterValue(ParamName, Amount);

        return Amount;
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get MaterialCollectionInstance!"));
    }
    return 0.0f;
}
FVector UWeatherLib::getWeatherWindDirection(UWorld* World)
{
    UMaterialParameterCollectionInstance* WeatherMaterialCollectionInstance = UWeatherLib::getWeatherMaterialCollectionInstance(World);
    if (WeatherMaterialCollectionInstance) {
        FName ParamName = GetWeatherParamVectorName(EWeatherParamVector::WEATHER_PARAM_VECTOR_WIND);
        if (ParamName == TEXT("")) {
            UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI got invalid paramname!"));
        }
        FLinearColor Direction;
        WeatherMaterialCollectionInstance->GetVectorParameterValue(ParamName, Direction); //SetScalarParameterValue(ParamName, Amount);

        return FVector(Direction);
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get MaterialCollectionInstance!"));
    }
    return FVector(0, 0, 0);
}
void UWeatherLib::setWeatherWindDirection(UWorld* World, FVector NewWind)
{
    UMaterialParameterCollectionInstance* WeatherMaterialCollectionInstance = UWeatherLib::getWeatherMaterialCollectionInstance(World);
    if (WeatherMaterialCollectionInstance) {
        FName ParamName = GetWeatherParamVectorName(EWeatherParamVector::WEATHER_PARAM_VECTOR_WIND);
        if (ParamName == TEXT("")) {
            UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI got invalid paramname!"));
        }
        WeatherMaterialCollectionInstance->SetVectorParameterValue(ParamName, NewWind);
    }
    else {
        UE_LOG(LogTemp, Warning, TEXT("Warning, WeatherAPI could NOT get MaterialCollectionInstance!"));
    }
}
bool UWeatherLib::getIsWeatherEnabled(UWorld* World)
{
    if (getWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_WEATHERENABLED) == 1.0f) {
        return true;
    }
    return false;
}
void UWeatherLib::setWeatherEnabled(UWorld* World, bool bEnabled)
{
    float Value = 0;
    if (bEnabled) {
        Value = 1;
    }
    setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_WEATHERENABLED, Value);
}
// BEIMING patch: 天气菜单已由项目内 O 键仿真配置面板（BeimingSim 模块 SimConfigWidget）取代。
// 以下函数保留空实现，仅为兼容 BP_LevelLoadButton 等仍引用它们的蓝图资产，保证 Cook 通过。
void UWeatherLib::showWeatherMenu(UWorld* World)
{
}
void UWeatherLib::hideWeatherMenu(UWorld* World)
{
}
bool UWeatherLib::isMenuVisible(UWorld* World)
{
    return false;
}
void UWeatherLib::toggleWeatherMenu(UWorld* World)
{
}
UWorld* UWeatherLib::widgetGetWorld(UUserWidget* Widget)
{
    if (Widget) {
        return Widget->GetWorld();
    }
    return NULL;
}
UWorld* UWeatherLib::actorGetWorld(AActor* Actor)
{
    if (Actor) {
        return Actor->GetWorld();
    }
    return NULL;
}
void UWeatherLib::setWeatherFog(AExponentialHeightFog* fog)
{
    weather_fog_ = fog;
}