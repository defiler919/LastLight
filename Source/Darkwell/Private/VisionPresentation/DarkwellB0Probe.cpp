#include "DarkwellB0Probe.h"
#include "Components/ActorComponent.h"
#include "HAL/IConsoleManager.h"
namespace Darkwell::B0
{
namespace
{
TAutoConsoleVariable<int32> Mode(TEXT("r.Darkwell.ObjectMemory.B0Probe"),0,
 TEXT("Diagnostic: 0 off, 1 reentry attribution, 2 same-frame registration batch. Not a production backend."));
const TCHAR* Names[Count]={TEXT("other"),TEXT("pixels"),TEXT("float16"),TEXT("signatures"),TEXT("cap_cpu"),
 TEXT("texture_object"),TEXT("cap_object"),TEXT("proxy_object"),TEXT("mesh_object"),TEXT("mid_object"),
 TEXT("register"),TEXT("material"),TEXT("texture_submit"),TEXT("cap_submit"),TEXT("visibility")};
// GT only. No UObject ownership survives ApplyPresentationDemand.
bool Active=false, InRecord=false;
int32 ActiveMode=0, Records=0, WholeRecords=0, QueuedCount=0;
ECost Current=Other;
double Last=0, Start=0, Total[Count]={}, RecordCosts[Count]={}, FlushMs=0;
TArray<UActorComponent*> Pending;
TArray<FString> Rows;
FString LastJson=TEXT("{}");
void Charge()
{
 const double Now=FPlatformTime::Seconds();
 const double Ms=(Now-Last)*1000.;
 Total[Current]+=Ms;
 RecordCosts[Current]+=Ms;
 Last=Now;
}
FString Costs(const double* Values)
{
 FString Out=TEXT("{");
 for(int32 I=0;I<Count;++I) Out+=FString::Printf(TEXT("%s\"%s\":%.6f"),I?TEXT(","):TEXT(""),Names[I],Values[I]);
 return Out+TEXT("}");
}
}
FScope::FScope(ECost Cost)
{
 Enabled=IsInGameThread() && InRecord;
 if(Enabled) {Charge();Previous=Current;Current=Cost;}
}
FScope::~FScope(){if(Enabled){Charge();Current=Previous;}}
FRecord::FRecord(bool Whole)
{
 Enabled=Active;
 if(Enabled){check(!InRecord);InRecord=true;Current=Other;FMemory::Memzero(RecordCosts);Last=FPlatformTime::Seconds();++Records;WholeRecords+=Whole;}
}
FRecord::~FRecord()
{
 if(Enabled){Charge();InRecord=false;Rows.Add(Costs(RecordCosts));}
}
void Begin()
{
 check(IsInGameThread() && !Active);
 ActiveMode=Mode.GetValueOnGameThread();Active=ActiveMode>0;
 Records=WholeRecords=QueuedCount=0;FlushMs=0;Pending.Reset();Rows.Reset();FMemory::Memzero(Total);
 LastJson=TEXT("{}");Start=FPlatformTime::Seconds();
}
void RegisterComponent(UActorComponent* Component)
{
 if(Active && InRecord && ActiveMode==2){Pending.Add(Component);++QueuedCount;}
 else Component->RegisterComponent();
}
void Flush(UWorld* World)
{
 if(Pending.IsEmpty()) return;
 TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_B0_BatchRegister);
 const double T=FPlatformTime::Seconds();
 {
 FRegisterComponentContext Context(World);
 for(auto* Component:Pending) if(IsValid(Component) && !Component->IsBeingDestroyed())
  Component->RegisterComponentWithWorld(World,&Context);
 Context.Process();
 }
 FlushMs=(FPlatformTime::Seconds()-T)*1000.;Pending.Reset();
}
void End()
{
 const double Wall=(FPlatformTime::Seconds()-Start)*1000.;
 if(Active && Records)
  LastJson=FString::Printf(TEXT("{\"mode\":%d,\"records\":%d,\"whole\":%d,\"queued\":%d,\"batch_ms\":%.6f,\"flush_ms\":%.6f,\"costs\":%s,\"record_costs\":[%s]}"),
   ActiveMode,Records,WholeRecords,QueuedCount,Wall,FlushMs,*Costs(Total),*FString::Join(Rows,TEXT(",")));
 Active=false;
}
FString Telemetry(){return LastJson;}
}
