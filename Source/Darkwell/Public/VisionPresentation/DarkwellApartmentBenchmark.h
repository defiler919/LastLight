#pragma once
#include "CoreMinimal.h"
class UWorld;
class ADarkwellObjectMemoryScene;
namespace Darkwell::ApartmentBenchmark
{
 // Development command-line-only, bounded real-input benchmark. Defaults to no-op.
 bool Tick(UWorld* World,ADarkwellObjectMemoryScene* Scene);
}
