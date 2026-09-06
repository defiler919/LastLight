#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DarkwellEditorDiagnostics.generated.h"

/** Transient editor automation only; no runtime or SightWeave dependency. */
UCLASS()
class DARKWELLEDITOR_API UDarkwellEditorDiagnostics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Darkwell|Diagnostics")
    static void StartPerformancePIE();
    UFUNCTION(BlueprintPure, Category="Darkwell|Diagnostics")
    static int32 GetRealtimeEditorViewportCount();
    UFUNCTION(BlueprintCallable, Category="Darkwell|Diagnostics")
    static void FocusPerformancePIE();
};
