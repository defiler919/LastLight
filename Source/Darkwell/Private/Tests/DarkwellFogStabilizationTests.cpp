#if WITH_DEV_AUTOMATION_TESTS
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "TextureResource.h"

namespace Darkwell::FogStabilizationTests
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
FDarkwellFogVisualSourceSnapshot SourceFor(int32 Case)
{
	FDarkwellFogVisualSourceSnapshot S;
	S.BodyRadiusCentimeters = 220; S.ConeRangeCentimeters = 1400; S.ConeHalfAngleDegrees = 32;
	S.bConeLegallyLive = Case != 0;
	S.ConeForward = FVector2D(FMath::Cos(Case * 1.73), FMath::Sin(Case * 1.73));
	S.BodyCenter = Case == 3 ? FVector2D(7750, 7850) : Case == 4 ? FVector2D(-8200, -7800) : FVector2D(37.125, -52.875);
	S.ConeOrigin = S.BodyCenter + FVector2D(17.5, -21.25);
	S.AuthorityRevision = Case + 1;
	return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCoverageBoundsTest,
	"Darkwell.FogVisual.CanonicalCoverage.ConservativeDrawSupport", Darkwell::FogStabilizationTests::Flags)
bool FDarkwellCoverageBoundsTest::RunTest(const FString&)
{
	FDarkwellFogVisualMapping M;
	M.WorldMin = FVector2D(-7800, -7900); M.WorldExtent = FVector2D(15600, 15800);
	M.InvWorldExtent = FVector2D(1.0/15600, 1.0/15800); M.TextureExtent = FIntPoint(6240,6320); M.CentimetersPerTexel = 2.5f;
	FRandomStream Random(7351);
	int32 Positive = 0, Missing = 0;
	for (int32 Case = 0; Case < 5; ++Case)
	{
		const auto S = Darkwell::FogStabilizationTests::SourceFor(Case);
		const FIntRect Rect = FDarkwellContinuousVisibilityBuilder::GetCoverageDrawRect(S, M, 2.5f);
		TestTrue(TEXT("Scissor is clamped"), Rect.Min.X >= 0 && Rect.Min.Y >= 0 && Rect.Max.X <= M.TextureExtent.X && Rect.Max.Y <= M.TextureExtent.Y);
		for (int32 I = 0; I < 200000; ++I)
		{
			const FIntPoint Pixel(Random.RandRange(0,6239),Random.RandRange(0,6319));
			const FVector2D Point = M.WorldMin + (FVector2D(Pixel) + FVector2D(0.5)) * 2.5;
			if (FDarkwellContinuousVisibilityBuilder::EvaluateNoOcclusionCoverage(S,Point,2.5f) > 0)
			{
				++Positive; if (!Rect.Contains(Pixel)) ++Missing;
			}
		}
	}
	TestTrue(TEXT("The oracle exercised positive legal samples"), Positive > 1000);
	TestEqual(TEXT("No positive analytic sample is excluded"), Missing, 0);
	return true;
}

// Separate selector: this oracle must run with a real RHI, never silently pass NullRHI.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCoverageGpuTest,
	"Darkwell.FogVisual.GPU.Stabilization.FullAndBoundedCoverage", Darkwell::FogStabilizationTests::Flags)
bool FDarkwellCoverageGpuTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddError(TEXT("GPU oracle requires a real rendering RHI")); return false; }
	IConsoleVariable* Full = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.FogVisual.Diagnostic.FullCoverageDraw"));
	if (!TestNotNull(TEXT("Full reference control"), Full)) return false;
	const int32 Previous = Full->GetInt();
	ON_SCOPE_EXIT { Full->Set(Previous, ECVF_SetByCode); };
	UWorld* World = NewObject<UWorld>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("CoverageGpuOracle")), RF_Transient);
	World->WorldType = EWorldType::Game;
	GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false).CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
	ON_SCOPE_EXIT { World->DestroyWorld(true); GEngine->DestroyWorldContext(World); FlushRenderingCommands(); };
	auto* Fog = World->GetSubsystem<UDarkwellFogVisualSubsystem>();
	const FBox2D Bounds(FVector2D(-7800,-7900), FVector2D(7800,7900));
	uint64 Revision = 100;
	for (int32 Case = 0; Case < 5; ++Case)
	{
		auto S = Darkwell::FogStabilizationTests::SourceFor(Case);
		TArray<FDarkwellFogVisualSegment> Walls;
		if (Case == 2) Walls.Add({S.BodyCenter + FVector2D(300,-900), S.BodyCenter + FVector2D(300,900)});
		Full->Set(1, ECVF_SetByCode); S.AuthorityRevision = ++Revision;
		if (!TestTrue(TEXT("Reference activation"), Fog->ActivateForWorld(Bounds,S,Walls))) return false;
		auto* Texture = Fog->GetLiveCoverageTexture();
		TestEqual(TEXT("Original texture width"), Texture->SizeX, 6240);
		TestEqual(TEXT("Original texture height"), Texture->SizeY, 6320);
		TestTrue(TEXT("Original mip chain retained"), Texture->bAutoGenerateMips);
		TArray<uint32> Reference;
		TArray<FLinearColor> Pixels;
		auto Read = [&](int32 Mip)
		{
			FReadSurfaceDataFlags ReadFlags(RCM_MinMax); ReadFlags.SetLinearToGamma(false); ReadFlags.SetMip(Mip);
			const FIntRect Rect(0,0,FMath::Max(1,Texture->SizeX >> Mip),FMath::Max(1,Texture->SizeY >> Mip));
			return Texture->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels,ReadFlags,Rect);
		};
		const int32 Mips = FMath::FloorLog2(FMath::Max(Texture->SizeX,Texture->SizeY)) + 1;
		for (int32 Mip = 0; Mip < Mips; ++Mip)
		{
			if (!TestTrue(TEXT("Full target readback"), Read(Mip))) return false;
			if (Mip == 0)
			{
				int32 Lit = 0, Edge = 0;
				for (const auto& P : Pixels) { Lit += P.R > 0; Edge += P.R > 0 && P.R < 1; }
				TestTrue(TEXT("Reference contains legal coverage and sub-texel edges"), Lit > 0 && Edge > 0);
			}
			Reference.Add(FCrc::MemCrc32(Pixels.GetData(),Pixels.Num()*sizeof(FLinearColor)));
		}
		// Poison the prior field far away: the candidate must clear stale pixels as well.
		auto Poison = S; Poison.BodyCenter = FVector2D(-4000,4000); Poison.ConeOrigin = Poison.BodyCenter; Poison.AuthorityRevision = ++Revision;
		Fog->UpdateSource(Poison); FlushRenderingCommands();
		Full->Set(0, ECVF_SetByCode); S.AuthorityRevision = ++Revision;
		TestTrue(TEXT("Bounded publication"), Fog->UpdateSource(S));
		for (int32 Mip = 0; Mip < Mips; ++Mip)
		{
			if (!TestTrue(TEXT("Bounded target readback"), Read(Mip))) return false;
			TestEqual(FString::Printf(TEXT("Case %d mip %d complete pixel CRC"),Case,Mip), FCrc::MemCrc32(Pixels.GetData(),Pixels.Num()*sizeof(FLinearColor)), Reference[Mip]);
		}
		AddInfo(FString::Printf(TEXT("Case %d: compared all %d full-resolution mip surfaces including stale-field clearing"),Case,Mips));
		Fog->Deactivate();
	}
	return true;
}
#endif
