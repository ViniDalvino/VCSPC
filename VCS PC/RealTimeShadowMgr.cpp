#include "StdAfx.h"
#include "RealTimeShadowMgr.h"

#include "Camera.h"
#include "Shadows.h"
#include "Ped.h"
#include "World.h"
#include "Tasks.h"
#include "PedIntelligence.h"

WRAPPER RwCamera* CShadowCamera::Create(int nSize) { WRAPARG(nSize); EAXJMP(0x705B60); }
WRAPPER void CShadowCamera::Destroy() { EAXJMP(0x705400); }
WRAPPER RwCamera* CShadowCamera::SetLight(RpLight* pLight) { WRAPARG(pLight); EAXJMP(0x705520); }
WRAPPER void CShadowCamera::MakeGradientRaster() { EAXJMP(0x705D20); }
WRAPPER RwCamera* CShadowCamera::SetCenter(RwV3d* pVector) { WRAPARG(pVector); EAXJMP(0x705590); }
WRAPPER RwRaster* CShadowCamera::RasterResample(RwRaster* pRaster) { WRAPARG(pRaster); EAXJMP(0x706070); }
WRAPPER RwRaster* CShadowCamera::RasterBlur(RwRaster* pRaster, int nPasses) { WRAPARG(pRaster); WRAPARG(nPasses); EAXJMP(0x706170); }
WRAPPER RwRaster* CShadowCamera::RasterGradient(RwRaster* pRaster) { WRAPARG(pRaster); EAXJMP(0x706330); }
WRAPPER void CShadowCamera::InvertRaster() { EAXJMP(0x705660); }

WRAPPER void CRealTimeShadow::Destroy() { EAXJMP(0x705990); }
WRAPPER bool CRealTimeShadow::SetShadowedObject(CEntity* pObject) { WRAPARG(pObject); EAXJMP(0x706520); }

WRAPPER void CRealTimeShadowManager::Update() { EAXJMP(0x706AB0); }

RpAtomic* ShadowCameraRenderCB_Vehicle(RpAtomic* pAtomic, void* pData)
{
	// Check if to render this basing on settings
	if ( reinterpret_cast<bool>(pData) )
	{
		// Reject if it's a LOD
		if ( strstr(GetFrameNodeName(RpAtomicGetFrame(pAtomic)), "_vlo") )
			return pAtomic;
	}
	else
	{
		// Reject if it's not a LOD
		if ( !strstr(GetFrameNodeName(RpAtomicGetFrame(pAtomic)), "_vlo") )
			return pAtomic;
	}

	RxPipeline*		pOldPipe;

	RpAtomicGetPipeline(pAtomic, &pOldPipe);
	RpAtomicSetPipeline(pAtomic, RpAtomicGetDefaultPipeline());

	ShadowCameraRenderCB(pAtomic, nullptr);

	RpAtomicSetPipeline(pAtomic, pOldPipe);

	return pAtomic;
}

RpAtomic* ShadowCameraRenderCB(RpAtomic* pAtomic, void* pData)
{
	UNREFERENCED_PARAMETER(pData);

	if ( RpAtomicGetFlags(pAtomic) & rpATOMICRENDER )
	{
		RpGeometry*	pGeometry = RpAtomicGetGeometry(pAtomic);
		RwUInt32	geometryFlags = RpGeometryGetFlags(pGeometry);

		RpGeometrySetFlags(pGeometry, geometryFlags & ~(rpGEOMETRYTEXTURED|rpGEOMETRYPRELIT|
						/*rpGEOMETRYNORMALS|*/rpGEOMETRYLIGHT|rpGEOMETRYMODULATEMATERIALCOLOR|rpGEOMETRYTEXTURED2));

		AtomicDefaultRenderCallBack(pAtomic);
		RpGeometrySetFlags(pGeometry, geometryFlags);
	}
	return pAtomic;
}

void CShadowCamera::ReInit()
{
	if ( m_pCamera )
	{
		RwInt32 nRasterWidth = RwRasterGetWidth(RwCameraGetRaster(m_pCamera));
		RwInt32	nRasterHeight = RwRasterGetHeight(RwCameraGetRaster(m_pCamera));

		RwRasterDestroy(RwCameraGetRaster(m_pCamera));
	
		RwRaster*	pNewRaster = RwRasterCreate(nRasterWidth, nRasterHeight, 0, rwRASTERTYPECAMERATEXTURE);
		RwCameraSetRaster(m_pCamera, pNewRaster);
		RwTextureSetRaster(m_pTexture, pNewRaster);
	}
}

RwCamera* CShadowCamera::Update(RpAtomic* pAtomic)
{
	if ( RpAtomicGetFlags(pAtomic) & rpATOMICRENDER )
	{
		RwRGBA	ClearColour = { 255, 255, 255, 0 };
		RwCameraClear(m_pCamera, &ClearColour, rwCAMERACLEARIMAGE|rwCAMERACLEARZ);

		if ( RwCameraBeginUpdate(m_pCamera ) )
		{
			RpGeometry*	pGeometry = RpAtomicGetGeometry(pAtomic);
			RwUInt32	geometryFlags = RpGeometryGetFlags(pGeometry);

			// Default pipeline
			RxPipeline*	pOldPipe;
			RpAtomicGetPipeline(pAtomic, &pOldPipe);
			RpAtomicSetPipeline(pAtomic, RpAtomicGetDefaultPipeline());


			RpGeometrySetFlags(pGeometry, geometryFlags & ~(rpGEOMETRYTEXTURED|rpGEOMETRYPRELIT|
							rpGEOMETRYLIGHT|rpGEOMETRYMODULATEMATERIALCOLOR|rpGEOMETRYTEXTURED2));
		
			AtomicDefaultRenderCallBack(pAtomic);

			RpAtomicSetPipeline(pAtomic, pOldPipe);
			RpGeometrySetFlags(pGeometry, geometryFlags);

			InvertRaster();
			RwCameraEndUpdate(m_pCamera);
		}
	}
	return m_pCamera;
}

RwCamera* CShadowCamera::Update(RpClump* pClump, CEntity* pEntity)
{
	RwRGBA	ClearColour = { 255, 255, 255, 0 };
	RwCameraClear(m_pCamera, &ClearColour, rwCAMERACLEARIMAGE|rwCAMERACLEARZ);

	if ( RwCameraBeginUpdate(m_pCamera ) )
	{
		if ( pEntity )
		{
			if ( pEntity->nType == 3 )
				static_cast<CPed*>(pEntity)->RenderForShadow(pClump, true);
			else if ( pEntity->nType == 2 )
				static_cast<CVehicle*>(pEntity)->RenderForShadow(pClump);
			else if ( pEntity->nType == 4 )
				RpClumpForAllAtomics(pClump, ShadowCameraRenderCB, nullptr);
			else
			{
				assert(!"Baad, unknown entity type in CShadowCamera::Update!");
			}
		}

		InvertRaster();
		RwCameraEndUpdate(m_pCamera);
	}
	return m_pCamera;
}

#include "Coronas.h"

RwTexture* CRealTimeShadow::Update()
{
	if ( m_pEntity->m_pRwObject && m_pEntity->bIveBeenRenderedOnce )
	{
		// Close enough to the object?
		CVector*	pObjPos = m_pEntity->GetCoords();
		CVector*	pCamPos = TheCamera.GetCoords();
		float		fMaxDist = CShadows::GetRealTimeShadowDistances(m_pEntity) * 1.1f;
		
		if ( (*pObjPos-*pCamPos).MagnitudeSqr() > fMaxDist*fMaxDist )
			return nullptr;

		if ( m_nRwObjectType == rpATOMIC )
			RwV3dTransformPoints(&m_BaseSphere.center, &m_BoundingSphere.center, 1, RwFrameGetMatrix(RpAtomicGetFrame(reinterpret_cast<RpAtomic*>(m_pEntity->m_pRwObject))));
		else if ( m_nRwObjectType == rpCLUMP )
			RwV3dTransformPoints(&m_BaseSphere.center, &m_BoundingSphere.center, 1, RwFrameGetMatrix(RpClumpGetFrame(reinterpret_cast<RpClump*>(m_pEntity->m_pRwObject))));

		m_Camera.SetCenter(&m_BaseSphere.center);
		if ( m_nRwObjectType == rpATOMIC )
			m_Camera.Update(reinterpret_cast<RpAtomic*>(m_pEntity->m_pRwObject));
		else if ( m_nRwObjectType == rpCLUMP )
			m_Camera.Update(reinterpret_cast<RpClump*>(m_pEntity->m_pRwObject), m_pEntity);


		/*CMatrix		Helper(RwFrameGetMatrix(RwCameraGetFrame(m_Camera.m_pCamera)));
		CVector		vecPos = *Helper.GetPos();
		CVector		vecRight = *Helper.GetPos() + *Helper.GetRight();
		CVector		vecUp = *Helper.GetPos() + *Helper.GetUp();
		CVector		vecAt = *Helper.GetPos() + *Helper.GetAt();

		// pos
		CCoronas::RegisterCorona((int)m_pEntity + 69, nullptr, 255, 255, 0, 255, vecPos, 0.25f, 1000.0f, gpCoronaTexture[1], 0, 0, 0, 0, 0.0f, false, 1.5f, false, 255, false, true);

		// right
		CCoronas::RegisterCorona((int)m_pEntity + 69 + 1, nullptr, 255, 0, 0, 255, vecRight, 0.25f, 1000.0f, gpCoronaTexture[1], 0, 0, 0, 0, 0.0f, false, 1.5f, false, 255, false, true);

		// up
		CCoronas::RegisterCorona((int)m_pEntity + 69 + 2, nullptr, 0, 255, 0, 255, vecUp, 0.25f, 1000.0f, gpCoronaTexture[1], 0, 0, 0, 0, 0.0f, false, 1.5f, false, 255, false, true);

		// at
		CCoronas::RegisterCorona((int)m_pEntity + 69 + 3, nullptr, 0, 0, 255, 255, vecAt, 0.25f, 1000.0f, gpCoronaTexture[1], 0, 0, 0, 0, 0.0f, false, 1.5f, false, 255, false, true);*/


		RwRaster*	pRaster = RwCameraGetRaster(m_Camera.m_pCamera);

		if ( m_bDrawResample )
			pRaster = m_ResampledCamera.RasterResample(pRaster);

		if ( m_dwBlurPasses )
			pRaster = m_bUsePlayerHelperCams ? g_realTimeShadowMan.m_BlurCamera_Player.RasterBlur(pRaster, m_dwBlurPasses) : g_realTimeShadowMan.m_BlurCamera.RasterBlur(pRaster, m_dwBlurPasses);

		if ( m_bDrawGradient )
			pRaster = m_bUsePlayerHelperCams ? g_realTimeShadowMan.m_GradientCamera_Player.RasterGradient(pRaster) : g_realTimeShadowMan.m_GradientCamera.RasterGradient(pRaster);

		return m_bDrawResample ? m_ResampledCamera.m_pTexture : m_Camera.m_pTexture;
	}
	return nullptr;
}

void CRealTimeShadow::Create(int nSize, int nSizeResampled, bool bResample, int nBlurPasses, bool bGradient, bool bUsePlayerCams)
{
	m_pLight = RpLightCreate(rpLIGHTDIRECTIONAL);

	if ( m_pLight )
	{
		const RwRGBAReal		LightColour = { 0.8f, 0.8f, 0.8f, 0.0f };
		RpLightSetColor(m_pLight, &LightColour);
		RpLightSetFrame(m_pLight, RwFrameCreate());

		if ( m_Camera.Create(nSize) )
		{
			m_bDrawResample = bResample;
			m_bDrawGradient = bGradient;
			m_dwBlurPasses = nBlurPasses;
			m_bUsePlayerHelperCams = bUsePlayerCams;
			if ( bResample )
			{
				if ( !m_ResampledCamera.Create(nSizeResampled) )
				{
					Destroy();
					return;
				}
			}
			m_Camera.SetLight(m_pLight);
		}
		else
		{
			Destroy();
			return;
		}
	}
}

// PRESETS
// Very Low: Player - static; peds - static
// Low: Player - 64x64, no resample, 2 blur passes; peds - static
// Medium: Player - 128x128, no resample, 3 blur passes; peds - player's Low
// High: Player - 512x512 resampled to 256x256, 4 blur passes; peds - player's Medium
// Very High: Player AND peds - 512x512 resampled to 256x256, 6 blur passes
static const tShadowQualitySettings			gShadowSettings[] = {
		// Low
		{ 6, 6, 2, true, false },
		// Medium
		{ 7, 7, 3, true, true },
		// High
		{ 9, 8, 4, true, true },
		// Very High
		{ 9, 8, 6, true, false }
	};

void CRealTimeShadowManager::Init()
{
	if ( !m_bInitialised )
	{
		for ( int i = 0; i < NUM_MAX_REALTIME_SHADOWS; i++ )
			m_pShadows[i] = new CRealTimeShadow;

		eShadowQuality		nShadowQuality = CShadows::GetShadowQuality();

		// Realtime shadows used?
		if ( nShadowQuality > SHADOW_QUALITY_LOWEST )
		{
			const tShadowQualitySettings*	pSets_Player;
			const tShadowQualitySettings*	pSets;

			switch ( nShadowQuality )
			{
			case SHADOW_QUALITY_LOW:
				pSets_Player = &gShadowSettings[0];
				pSets = nullptr;
				break;
			case SHADOW_QUALITY_MEDIUM:
				pSets_Player = &gShadowSettings[1];
				pSets = &gShadowSettings[0];
				break;
			case SHADOW_QUALITY_HIGH:
				pSets_Player = &gShadowSettings[2];
				pSets = &gShadowSettings[1];
				break;
			case SHADOW_QUALITY_HIGHEST:
				pSets_Player = &gShadowSettings[3];
				pSets = &gShadowSettings[3];
				break;
			default:
				pSets_Player = nullptr;
				pSets = nullptr;
				break;
			}

			for ( int i = 0; i < NUM_MAX_REALTIME_SHADOWS; i++ )
			{
				if ( !i )
					m_pShadows[0]->Create(pSets_Player->nQuality, pSets_Player->nQualityAfterResample, 
									pSets_Player->nQuality != pSets_Player->nQualityAfterResample,
									pSets_Player->nBlurPasses, pSets_Player->bUseGradient,
									pSets_Player->bDedicatedCamsForPlayer);
				else
				{
					if ( pSets )
						m_pShadows[i]->Create(pSets->nQuality, pSets->nQualityAfterResample, 
									pSets->nQuality != pSets->nQualityAfterResample,
									pSets->nBlurPasses, pSets->bUseGradient,
									false);
				}
			}

			if ( !pSets )
				pSets = pSets_Player;

			m_BlurCamera.Create(pSets->nQualityAfterResample);
			m_GradientCamera.Create(pSets->nQualityAfterResample);
			m_GradientCamera.MakeGradientRaster();

			if ( pSets_Player->bDedicatedCamsForPlayer )
			{
				m_BlurCamera_Player.Create(pSets_Player->nQualityAfterResample);
				m_GradientCamera_Player.Create(pSets_Player->nQualityAfterResample);
				m_GradientCamera_Player.MakeGradientRaster();
				m_bPlayerHelperCamsInUse = true;
			}
			else
				m_bPlayerHelperCamsInUse = false;
		}

		m_bInitialised = true;
	}
}

void CRealTimeShadowManager::Exit()
{
	if ( m_bInitialised )
	{
		for ( int i = 0; i < NUM_MAX_REALTIME_SHADOWS; i++ )
			delete m_pShadows[i];

		m_BlurCamera.Destroy();
		m_GradientCamera.Destroy();

		if ( m_bPlayerHelperCamsInUse )
		{
			m_BlurCamera_Player.Destroy();
			m_GradientCamera_Player.Destroy();
			m_bPlayerHelperCamsInUse = false;
		}
		m_bInitialised = false;
	}
}

void CRealTimeShadowManager::DoShadowThisFrame(CEntity* pEntity)
{
	if ( m_bNeedsReinit )
		return;

	bool			bRenderAtLowDetails;
	eShadowQuality	nShadowQuality = CShadows::GetShadowQuality();

	if ( pEntity->nType == 3 )
		bRenderAtLowDetails = static_cast<CPed*>(pEntity)->pedType == 0;
	else
		bRenderAtLowDetails = false;

	if ( nShadowQuality > SHADOW_QUALITY_LOW || (nShadowQuality == SHADOW_QUALITY_LOW && bRenderAtLowDetails) )
	{
		CRealTimeShadow*	pEntityShadow = pEntity->GetRealTimeShadow();
		//assert(pEntityShadow < (CRealTimeShadow*)0x15000000);
		if ( pEntityShadow )
			pEntityShadow->SetRenderedThisFrame();
		else
			GetRealTimeShadow(pEntity);
	}
}

void CRealTimeShadowManager::GetRealTimeShadow(CEntity* pEntity)
{
	bool	bIsPlayer;
	bool	bRender = true;

	if ( pEntity->nType == 3 )
		bIsPlayer = static_cast<CPed*>(pEntity)->pedType == 0;
	else
		bIsPlayer = false;

	/*if ( pEntity->nType != 3 || !bIsPlayer )
	{
		CPed*	pPlayerPed = CWorld::Players[CWorld::PlayerInFocus].GetPed();

		if ( pPlayerPed->pedFlags.bInVehicle )
		{
			/*CVehicle*	pVehicle = pPlayerPed->pVehicle;
			if ( pVehicle )
				bRender = pVehicle->GetLinearVelocity().MagnitudeSqr() <= 0.3f * 0.3f;*/
//			bIsPlayer = pPlayerPed->pVehicle == pEntity;
//		}
//	}

	if ( m_bInitialised && bRender )
	{
		CRealTimeShadow*	pOutShadow;
		if ( bIsPlayer )
			pOutShadow = m_pShadows[0];
		else
		{
			pOutShadow = nullptr;
			for ( int i = 1; i < NUM_MAX_REALTIME_SHADOWS; i++ )
			{
				if ( !m_pShadows[i]->GetOwner() )
				{
					pOutShadow = m_pShadows[i];
					break;
				}
			}

			// Debug
			if ( !pOutShadow )
			{
				assert(!"Too many real time shadows at once, consider increasing limits.");
			}
		}

		if ( pOutShadow )
		{
			pOutShadow->SetShadowedObject(pEntity);
			pOutShadow->ResetIntensity();
			pEntity->SetRealTimeShadow(pOutShadow);
			pOutShadow->SetRenderedThisFrame();
		}
	}
}

void CRealTimeShadowManager::ReInit()
{
	if ( !m_bNewSettings )
	{
		for ( int i = 0; i < NUM_MAX_REALTIME_SHADOWS; i++ )
			m_pShadows[i]->ReInit();

		m_BlurCamera.ReInit();
		m_GradientCamera.ReInit();
		m_GradientCamera.MakeGradientRaster();

		if ( m_bPlayerHelperCamsInUse )
		{
			m_BlurCamera_Player.ReInit();
			m_GradientCamera_Player.ReInit();
			m_GradientCamera_Player.MakeGradientRaster();
		}
	}
	else
	{
		const tShadowQualitySettings*	pSets_Player;
		const tShadowQualitySettings*	pSets;
		
		switch ( CShadows::GetShadowQuality() )
		{
		case SHADOW_QUALITY_LOW:
			pSets_Player = &gShadowSettings[0];
			pSets = nullptr;
			break;
		case SHADOW_QUALITY_MEDIUM:
			pSets_Player = &gShadowSettings[1];
			pSets = &gShadowSettings[0];
			break;
		case SHADOW_QUALITY_HIGH:
			pSets_Player = &gShadowSettings[2];
			pSets = &gShadowSettings[1];
			break;
		case SHADOW_QUALITY_HIGHEST:
			pSets_Player = &gShadowSettings[3];
			pSets = &gShadowSettings[3];
			break;
		default:
			pSets_Player = nullptr;
			pSets = nullptr;
			break;
		}

		for ( int i = 0; i < NUM_MAX_REALTIME_SHADOWS; i++ )
		{
			CEntity*	pOldOwner = m_pShadows[i]->GetOwner();
			m_pShadows[i]->Destroy();

			if ( !i )
			{
				if ( pSets_Player )	
				{
					m_pShadows[0]->Create(pSets_Player->nQuality, pSets_Player->nQualityAfterResample, 
								pSets_Player->nQuality != pSets_Player->nQualityAfterResample,
								pSets_Player->nBlurPasses, pSets_Player->bUseGradient,
								pSets_Player->bDedicatedCamsForPlayer);

					if ( pOldOwner )
						m_pShadows[i]->SetShadowedObject(pOldOwner);
				}
				else
				{
					if ( pOldOwner )
						pOldOwner->SetRealTimeShadow(nullptr);
				}
			}
			else
			{
				if ( pSets )
				{
					m_pShadows[i]->Create(pSets->nQuality, pSets->nQualityAfterResample, 
								pSets->nQuality != pSets->nQualityAfterResample,
								pSets->nBlurPasses, pSets->bUseGradient,
								false);

					if ( pOldOwner )
						m_pShadows[i]->SetShadowedObject(pOldOwner);
				}
				else
				{
					if ( pOldOwner )
						pOldOwner->SetRealTimeShadow(nullptr);
				}
			}

			if ( !pSets )
				pSets = pSets_Player;
		}

		m_BlurCamera.Destroy();
		m_GradientCamera.Destroy();

		if ( m_bPlayerHelperCamsInUse )
		{
			m_BlurCamera_Player.Destroy();
			m_GradientCamera_Player.Destroy();
		}

		if ( pSets )
		{
			m_BlurCamera.Create(pSets->nQualityAfterResample);
			m_GradientCamera.Create(pSets->nQualityAfterResample);
			m_GradientCamera.MakeGradientRaster();

			if ( pSets_Player->bDedicatedCamsForPlayer )
			{
				m_BlurCamera_Player.Create(pSets_Player->nQualityAfterResample);
				m_GradientCamera_Player.Create(pSets_Player->nQualityAfterResample);
				m_GradientCamera_Player.MakeGradientRaster();
				m_bPlayerHelperCamsInUse = true;
			}
			else
				m_bPlayerHelperCamsInUse = false;
		}

		m_bNewSettings = false;
	}
}

void CRealTimeShadowManager::KeepBuildingShadowsAlive()
{
	if ( m_bInitialised && CShadows::GetShadowQuality() > SHADOW_QUALITY_MEDIUM )
	{
		CVector		vecCamCoords = *TheCamera.GetCoords();
		for ( int i = 0; i < NUM_MAX_REALTIME_SHADOWS; i++ )
		{
			if ( !m_pShadows[i]->GetRenderedThisFrame() )
			{
				CEntity*	pShadowOwner = m_pShadows[i]->GetOwner();
				if ( pShadowOwner )
				{
					if ( pShadowOwner->nType == 1 )
					{
						if ( (*pShadowOwner->GetCoords() - vecCamCoords).MagnitudeSqr() < 45.0*45.0 )
							m_pShadows[i]->SetRenderedThisFrame();
					}
				}
			}
		}
	}
}

void CRealTimeShadowManager::ReturnRealTimeShadow(CRealTimeShadow* pShadow)
{
	if ( m_bInitialised && pShadow->GetOwner() )
	{
		pShadow->GetOwner()->SetRealTimeShadow(nullptr);
		pShadow->ClearOwner();
	}
}

static void BuildingShadowsKeep()
{
	g_realTimeShadowMan.KeepBuildingShadowsAlive();
}

static void UpdateShadowsHack(RwCamera* camera)
{
	g_realTimeShadowMan.Update();
	
	//((void(*)())0x707F40)();
	RwCameraBeginUpdate(camera);
}

static void __declspec(naked) ReturnShadowHack()
{
	_asm
	{
		push	ecx
		mov		ecx, edi
		call	CRealTimeShadowManager::ReturnRealTimeShadow
		push	706B70h
		retn
	}
}


static StaticPatcher	Patcher([](){ 
						Memory::InjectHook(0x706B19, ReturnShadowHack, PATCH_JUMP);

						Memory::InjectHook(0x7067C0, &CRealTimeShadowManager::Init, PATCH_JUMP);
						Memory::InjectHook(0x706A60, &CRealTimeShadowManager::Exit, PATCH_JUMP);
						Memory::InjectHook(0x706BA0, &CRealTimeShadowManager::DoShadowThisFrame, PATCH_JUMP);
						Memory::InjectHook(0x706AC2, &CRealTimeShadowManager::ReInit);
						Memory::InjectHook(0x706B29, &CRealTimeShadow::Update);
						Memory::InjectHook(0x5539FC, BuildingShadowsKeep);

						// Increased shadows limit
						Memory::Patch<const void*>(0x45D412, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x53BE63, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x53C63F, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x53C9E5, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x53EA09, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x542487, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x5B1F38, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x5BA478, &g_realTimeShadowMan);
						Memory::Patch<const void*>(0x5E68A4, &g_realTimeShadowMan);
						//Memory::Patch<const void*>(0x854980, &g_realTimeShadowMan);
						//Memory::Patch<const void*>(0x856AD0, &g_realTimeShadowMan);

						// Shadows rendering AFTER RenderScene
						//Memory::InjectHook(0x53E0B9, UpdateShadowsHack);
						//Memory::Nop(0x53EA0D, 5);

						Memory::Patch<const void*>(0x5BA12C, &g_realTimeShadowMan.m_bInitialised);
						Memory::Patch<const void*>(0x5BA137, &g_realTimeShadowMan.m_bNeedsReinit);

						Memory::Patch<DWORD>(0x706AD4, NUM_MAX_REALTIME_SHADOWS);
						Memory::Patch<DWORD>(0x706B80, NUM_MAX_REALTIME_SHADOWS);

						Memory::Patch<BYTE>(0x706AF3, 0xB8);
						Memory::Patch<DWORD>(0x706AF4, SHADOWS_MAX_INTENSITY);
						Memory::Patch<WORD>(0x706AF8, 0x15EB);
									});