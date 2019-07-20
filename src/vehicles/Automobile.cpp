#include "common.h"
#include "main.h"
#include "patcher.h"
#include "General.h"
#include "RwHelper.h"
#include "Pad.h"
#include "ModelIndices.h"
#include "VisibilityPlugins.h"
#include "DMAudio.h"
#include "Camera.h"
#include "Darkel.h"
#include "Rubbish.h"
#include "Fire.h"
#include "Explosion.h"
#include "Particle.h"
#include "World.h"
#include "SurfaceTable.h"
#include "HandlingMgr.h"
#include "Record.h"
#include "Remote.h"
#include "Population.h"
#include "CarCtrl.h"
#include "CarAI.h"
#include "Garages.h"
#include "PathFind.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "Object.h"
#include "Automobile.h"

bool bAllCarCheat;	// unused

RwObject *GetCurrentAtomicObjectCB(RwObject *object, void *data);

bool &CAutomobile::m_sAllTaxiLights = *(bool*)0x95CD21;

CAutomobile::CAutomobile(int32 id, uint8 CreatedBy)
 : CVehicle(CreatedBy)
{
	int i;

	m_vehType = VEHICLE_TYPE_CAR;

	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(id);
	m_fFireBlowUpTimer = 0.0f;
	field_4E0 = 0;
	bTaxiLight = m_sAllTaxiLights;
	m_auto_flagA20 = false;
	m_auto_flagA40 = false;
	m_auto_flagA80 = false;

	SetModelIndex(id);

	pHandling = mod_HandlingManager.GetHandlingData((eHandlingId)mi->m_handlingId);

	field_49C = 20.0f;
	field_4D8 = 0;

	mi->ChooseVehicleColour(m_currentColour1, m_currentColour2);

	bIsVan = !!(pHandling->Flags & HANDLING_IS_VAN);
	bIsBig = !!(pHandling->Flags & HANDLING_IS_BIG);
	bIsBus = !!(pHandling->Flags & HANDLING_IS_BUS);
	bLowVehicle = !!(pHandling->Flags & HANDLING_IS_LOW);

	// Doors
	if(bIsBus){
		Doors[DOOR_FRONT_LEFT].Init(-HALFPI, 0.0f, 0, 2);
		Doors[DOOR_FRONT_RIGHT].Init(0.0f, HALFPI, 1, 2);
	}else{
		Doors[DOOR_FRONT_LEFT].Init(-PI*0.4f, 0.0f, 0, 2);
		Doors[DOOR_FRONT_RIGHT].Init(0.0f, PI*0.4f, 1, 2);
	}
	if(bIsVan){
		Doors[DOOR_REAR_LEFT].Init(-HALFPI, 0.0f, 1, 2);
		Doors[DOOR_REAR_RIGHT].Init(0.0f, HALFPI, 0, 2);
	}else{
		Doors[DOOR_REAR_LEFT].Init(-PI*0.4f, 0.0f, 0, 2);
		Doors[DOOR_REAR_RIGHT].Init(0.0f, PI*0.4f, 1, 2);
	}
	if(pHandling->Flags & HANDLING_REV_BONNET)
		Doors[DOOR_BONNET].Init(-PI*0.3f, 0.0f, 1, 0);
	else
		Doors[DOOR_BONNET].Init(0.0f, PI*0.3f, 1, 0);
	if(pHandling->Flags & HANDLING_HANGING_BOOT)
		Doors[DOOR_BOOT].Init(PI*0.4f, 0.0f, 0, 0);
	else if(pHandling->Flags & HANDLING_TAILGATE_BOOT)
		Doors[DOOR_BOOT].Init(0.0, HALFPI, 1, 0);
	else
		Doors[DOOR_BOOT].Init(-PI*0.3f, 0.0f, 1, 0);
	if(pHandling->Flags & HANDLING_NO_DOORS){
		Damage.SetDoorStatus(DOOR_FRONT_LEFT, DOOR_STATUS_MISSING);
		Damage.SetDoorStatus(DOOR_FRONT_RIGHT, DOOR_STATUS_MISSING);
		Damage.SetDoorStatus(DOOR_REAR_LEFT, DOOR_STATUS_MISSING);
		Damage.SetDoorStatus(DOOR_REAR_RIGHT, DOOR_STATUS_MISSING);
	}

	for(i = 0; i < 6; i++)
		m_randomValues[i] = CGeneral::GetRandomNumberInRange(-0.15f, 0.15f);

	m_fMass = pHandling->fMass;
	m_fTurnMass = pHandling->fTurnMass;
	m_vecCentreOfMass = pHandling->CentreOfMass;
	m_fAirResistance = pHandling->Dimension.x*pHandling->Dimension.z/m_fMass;
	m_fElasticity = 0.05f;
	m_fBuoyancy = pHandling->fBuoyancy;

	m_nBusDoorTimerEnd = 0;
	m_nBusDoorTimerStart = 0;

	m_fSteerAngle = 0.0f;
	m_fGasPedal = 0.0f;
	m_fBrakePedal = 0.0f;
	m_pSetOnFireEntity = nil;
	field_594 = 0;
	bNotDamagedUpsideDown = false;
	bMoreResistantToDamage = false;
	m_fVelocityChangeForAudio = 0.f;
	field_4E2 = 0;

	for(i = 0; i < 4; i++){
		m_aGroundPhysical[i] = nil;
		m_aGroundOffset[i] = CVector(0.0f, 0.0f, 0.0f);
		m_aSuspensionSpringRatio[i] = 1.0f;
		m_aSuspensionSpringRatioPrev[i] = m_aSuspensionSpringRatio[i];
		m_aWheelTimer[i] = 0.0f;
		m_aWheelRotation[i] = 0.0f;
		m_aWheelSpeed[i] = 0.0f;
		m_aWheelState[i] = WHEEL_STATE_0;
		m_aWheelSkidmarkMuddy[i] = false;
		m_aWheelSkidmarkBloody[i] = false;
	}

	m_nWheelsOnGround = 0;
	m_nDriveWheelsOnGround = 0;
	m_nDriveWheelsOnGroundPrev = 0;
	m_fHeightAboveRoad = 0.0f;
	m_fTraction = 1.0f;

	CColModel *colModel = mi->GetColModel();
	if(colModel->lines == nil){
		colModel->lines = (CColLine*)RwMalloc(4*sizeof(CColLine));
		colModel->numLines = 4;
	}

	SetupSuspensionLines();

	m_status = STATUS_SIMPLE;
	bUseCollisionRecords = true;

	m_nNumPassengers = 0;

	m_bombType = CARBOMB_NONE;
	bHadDriver = false;
	m_pBombRigger = nil;

	if(m_nDoorLock == CARLOCK_UNLOCKED &&
	   (id == MI_POLICE || id == MI_ENFORCER || id == MI_RHINO))
		m_nDoorLock = CARLOCK_LOCKED_INITIALLY;

	m_fCarGunLR = 0.0f;
	m_fCarGunUD = 0.05f;
	m_fWindScreenRotation = 0.0f;
	m_weaponThingA = 0.0f;
	m_weaponThingB = m_weaponThingA;

	if(GetModelIndex() == MI_DODO){
		RpAtomicSetFlags(GetFirstObject(m_aCarNodes[CAR_WHEEL_LF]), 0);
		CMatrix mat1;
		mat1.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RF]));
		CMatrix mat2(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LF]));
		mat1.GetPosition() += CVector(mat2.GetPosition().x + 0.1f, 0.0f, mat2.GetPosition().z);
		mat1.UpdateRW();
	}else if(GetModelIndex() == MI_MIAMI_SPARROW || GetModelIndex() == MI_MIAMI_RCRAIDER){
		RpAtomicSetFlags(GetFirstObject(m_aCarNodes[CAR_WHEEL_LF]), 0);
		RpAtomicSetFlags(GetFirstObject(m_aCarNodes[CAR_WHEEL_RF]), 0);
		RpAtomicSetFlags(GetFirstObject(m_aCarNodes[CAR_WHEEL_LB]), 0);
		RpAtomicSetFlags(GetFirstObject(m_aCarNodes[CAR_WHEEL_RB]), 0);
	}else if(GetModelIndex() == MI_RHINO){
		bExplosionProof = true;
		bBulletProof = true;
	}
}


void
CAutomobile::SetModelIndex(uint32 id)
{
	CVehicle::SetModelIndex(id);
	SetupModelNodes();
}

CVector vecDAMAGE_ENGINE_POS_SMALL(-0.1f, -0.1f, 0.0f);
CVector vecDAMAGE_ENGINE_POS_BIG(-0.5f, -0.3f, 0.0f);

//WRAPPER void CAutomobile::ProcessControl(void) { EAXJMP(0x531470); }
void
CAutomobile::ProcessControl(void)
{
	int i;
	CColModel *colModel;

	if(m_veh_flagC80)
		colModel = &CWorld::Players[CWorld::PlayerInFocus].m_ColModel;
	else
		colModel = GetColModel();
	bWarnedPeds = false;

	// skip if the collision isn't for the current level
	if(colModel->level > LEVEL_NONE && colModel->level != CCollision::ms_collisionInMemory)
		return;

	// Improve grip of vehicles in certain cases
	bool strongGrip1 = false;
	bool strongGrip2 = false;
	if(FindPlayerVehicle() && this != FindPlayerVehicle()){
		switch(AutoPilot.m_nCarMission){
		case MISSION_RAMPLAYER_FARAWAY:
		case MISSION_RAMPLAYER_CLOSE:
		case MISSION_BLOCKPLAYER_FARAWAY:
		case MISSION_BLOCKPLAYER_CLOSE:
			if(FindPlayerSpeed().Magnitude() > 0.3f){
				strongGrip1 = true;
				if(FindPlayerSpeed().Magnitude() > 0.4f){
					if(m_vecMoveSpeed.Magnitude() < 0.3f)
						strongGrip2 = true;
				}else{
					if((GetPosition() - FindPlayerCoors()).Magnitude() > 50.0f)
						strongGrip2 = true;
				}
			}
		}
	}

	if(bIsBus)
		ProcessAutoBusDoors();

	ProcessCarAlarm();

	// Scan if this car is committing a crime that the police can see
	if(m_status != STATUS_ABANDONED && m_status != STATUS_WRECKED &&
	   m_status != STATUS_PLAYER && m_status != STATUS_PLAYER_REMOTE && m_status != STATUS_PLAYER_DISABLED){
		switch(GetModelIndex())
		case MI_FBICAR:
		case MI_POLICE:
		case MI_ENFORCER:
		case MI_SECURICA:
		case MI_RHINO:
		case MI_BARRACKS:
			ScanForCrimes();
	}

	// Process driver
	if(pDriver){
		if(!bHadDriver && m_bombType == CARBOMB_ONIGNITIONACTIVE){
			// If someone enters the car and there is a bomb, detonate
			m_nBombTimer = 1000;
			m_pBlowUpEntity = m_pBombRigger;
			if(m_pBlowUpEntity)
				m_pBlowUpEntity->RegisterReference((CEntity**)&m_pBlowUpEntity);
			DMAudio.PlayOneShot(m_audioEntityId, SOUND_BOMB_TICK, 1.0f);
		}
		bHadDriver = true;

		if(IsUpsideDown() && CanPedEnterCar()){
			if(!pDriver->IsPlayer() &&
			   !(pDriver->m_leader && pDriver->m_leader->bInVehicle) &&
			   pDriver->CharCreatedBy != MISSION_CHAR)
				pDriver->SetObjective(OBJECTIVE_LEAVE_VEHICLE, this);
		}
	}else
		bHadDriver = false;

	// Process passengers
	if(m_nNumPassengers != 0 && IsUpsideDown() && CanPedEnterCar()){
		for(i = 0; i < m_nNumMaxPassengers; i++)
			if(pPassengers[i])
				if(!pPassengers[i]->IsPlayer() &&
				   !(pPassengers[i]->m_leader && pPassengers[i]->m_leader->bInVehicle) &&
				   pPassengers[i]->CharCreatedBy != MISSION_CHAR)
					pPassengers[i]->SetObjective(OBJECTIVE_LEAVE_VEHICLE, this);
	}

	CRubbish::StirUp(this);

	// blend in clump
	int clumpAlpha = CVisibilityPlugins::GetClumpAlpha((RpClump*)m_rwObject);
	if(bFadeOut){
		clumpAlpha -= 8;
		if(clumpAlpha < 0)
			clumpAlpha = 0;
	}else if(clumpAlpha < 255){
		clumpAlpha += 16;
		if(clumpAlpha > 255)
			clumpAlpha = 255;
	}
	CVisibilityPlugins::SetClumpAlpha((RpClump*)m_rwObject, clumpAlpha);

	AutoPilot.m_flag1 = false;
	AutoPilot.m_flag2 = false;

	// Set Center of Mass to make car more stable
	if(strongGrip1 || bCheat3)
		m_vecCentreOfMass.z = 0.3f*m_aSuspensionSpringLength[0] + -1.0*m_fHeightAboveRoad;
	else if(pHandling->Flags & HANDLING_NONPLAYER_STABILISER && m_status == STATUS_PHYSICS)
		m_vecCentreOfMass.z = pHandling->CentreOfMass.z - 0.2f*pHandling->Dimension.z;
	else
		m_vecCentreOfMass.z = pHandling->CentreOfMass.z;

	// Process depending on status

	bool playerRemote = false;
	switch(m_status){
	case STATUS_PLAYER_REMOTE:
		if(CPad::GetPad(0)->WeaponJustDown()){
			BlowUpCar(FindPlayerPed());
			CRemote::TakeRemoteControlledCarFromPlayer();
		}

		if(GetModelIndex() == MI_RCBANDIT){
			CVector pos = GetPosition();
			// FindPlayerCoors unused
			if(RcbanditCheckHitWheels() || bIsInWater || CPopulation::IsPointInSafeZone(&pos)){
				if(CPopulation::IsPointInSafeZone(&pos))
					CGarages::TriggerMessage("HM2_5", -1, 5000, -1);
				CRemote::TakeRemoteControlledCarFromPlayer();
				BlowUpCar(FindPlayerPed());
			}
		}

		if(CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle == this)
			playerRemote = true;
		// fall through
	case STATUS_PLAYER:
		if(playerRemote ||
		   pDriver && pDriver->GetPedState() != PED_EXIT_CAR && pDriver->GetPedState() != PED_DRAG_FROM_CAR){
			// process control input if controlled by player
			if(playerRemote || pDriver->m_nPedType == PEDTYPE_PLAYER1)
				ProcessControlInputs(0);

			PruneReferences();

			if(m_status == STATUS_PLAYER && CRecordDataForChase::Status != RECORDSTATE_1)
				DoDriveByShootings();
		}
		break;

	case STATUS_SIMPLE:
		CCarAI::UpdateCarAI(this);
		CPhysical::ProcessControl();
		CCarCtrl::UpdateCarOnRails(this);

		m_nWheelsOnGround = 4;
		m_nDriveWheelsOnGroundPrev = m_nDriveWheelsOnGround;
		m_nDriveWheelsOnGround = 4;

		pHandling->Transmission.CalculateGearForSimpleCar(AutoPilot.m_fMaxTrafficSpeed/50.0f, m_nCurrentGear);

		{
		float wheelRot = ProcessWheelRotation(WHEEL_STATE_0, GetForward(), m_vecMoveSpeed, 0.35f);
		for(i = 0; i < 4; i++)
			m_aWheelRotation[i] += wheelRot;
		}

		PlayHornIfNecessary();
		ReduceHornCounter();
		bVehicleColProcessed = false;
		// that's all we do for simple vehicles
		return;

	case STATUS_PHYSICS:
		CCarAI::UpdateCarAI(this);
		CCarCtrl::SteerAICarWithPhysics(this);
		PlayHornIfNecessary();
		break;

	case STATUS_ABANDONED:
		m_fBrakePedal = 0.2f;
		bIsHandbrakeOn = false;

		m_fSteerAngle = 0.0f;
		m_fGasPedal = 0.0f;
		m_nCarHornTimer = 0;
		break;

	case STATUS_WRECKED:
		m_fBrakePedal = 0.05f;
		bIsHandbrakeOn = true;

		m_fSteerAngle = 0.0f;
		m_fGasPedal = 0.0f;
		m_nCarHornTimer = 0;
		break;

	case STATUS_PLAYER_DISABLED:
		m_fBrakePedal = 1.0f;
		bIsHandbrakeOn = true;

		m_fSteerAngle = 0.0f;
		m_fGasPedal = 0.0f;
		m_nCarHornTimer = 0;
		break;
	}

	// what's going on here?
	if(GetPosition().z < -0.6f &&
	   Abs(m_vecMoveSpeed.x) < 0.05f &&
	   Abs(m_vecMoveSpeed.y) < 0.05f)
		m_vecTurnSpeed *= Pow(0.95f, CTimer::GetTimeStep());

	// Skip physics if object is found to have been static recently
	bool skipPhysics = false;
	if(!bIsStuck && (m_status == STATUS_ABANDONED || m_status == STATUS_WRECKED)){
		bool makeStatic = false;
		float moveSpeedLimit, turnSpeedLimit, distanceLimit;

		if(!bVehicleColProcessed &&
		   m_vecMoveSpeed.IsZero() &&
		// BUG? m_aSuspensionSpringRatioPrev[3] is checked twice in the game. also, why 3?
		   m_aSuspensionSpringRatioPrev[3] != 1.0f)
			makeStatic = true;

		if(m_status == STATUS_WRECKED){
			moveSpeedLimit = 0.006f;
			turnSpeedLimit = 0.0015f;
			distanceLimit = 0.015f;
		}else{
			moveSpeedLimit = 0.003f;
			turnSpeedLimit = 0.0009f;
			distanceLimit = 0.005f;
		}

		m_vecMoveSpeedAvg = (m_vecMoveSpeedAvg + m_vecMoveSpeed)/2.0f;
		m_vecTurnSpeedAvg = (m_vecTurnSpeedAvg + m_vecTurnSpeed)/2.0f;

		if(m_vecMoveSpeedAvg.MagnitudeSqr() <= sq(moveSpeedLimit*CTimer::GetTimeStep()) &&
		   m_vecTurnSpeedAvg.MagnitudeSqr() <= sq(turnSpeedLimit*CTimer::GetTimeStep()) &&
		   m_fDistanceTravelled < distanceLimit ||
		   makeStatic){
			m_nStaticFrames++;

			if(m_nStaticFrames > 10 || makeStatic)
				if(!CCarCtrl::MapCouldMoveInThisArea(GetPosition().x, GetPosition().y)){
					if(!makeStatic || m_nStaticFrames > 10)
						m_nStaticFrames = 10;

					skipPhysics = true;

					m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
					m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
				}
		}else
			m_nStaticFrames = 0;
	}

	// Postpone
	for(i = 0; i < 4; i++)
		if(m_aGroundPhysical[i] && !CWorld::bForceProcessControl && m_aGroundPhysical[i]->bIsInSafePosition){
			bWasPostponed = true;
			return;
		}

	VehicleDamage(0.0f, 0);

	// special control
	switch(GetModelIndex()){
	case MI_FIRETRUCK:
		FireTruckControl();
		break;
	case MI_RHINO:
		TankControl();
		BlowUpCarsInPath();
		break;
	case MI_YARDIE:
	// beta also had esperanto here it seems
		HydraulicControl();
		break;
	default:
		if(CVehicle::bCheat3){
			// Make vehicle jump when horn is sounded
			if(m_status == STATUS_PLAYER && m_vecMoveSpeed.MagnitudeSqr() > sq(0.2f) &&
			// BUG: game checks [0] four times, instead of all wheels
			   m_aSuspensionSpringRatio[0] < 1.0f &&
			   CPad::GetPad(0)->HornJustDown()){

				DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_HYDRALIC_1, 0.0f);
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_JUMP, 1.0f);

				CParticle::AddParticle(PARTICLE_ENGINE_STEAM,
					m_aWheelColPoints[0].point + 0.5f*GetUp(),
					1.3f*m_vecMoveSpeed, nil, 2.5f);
				CParticle::AddParticle(PARTICLE_ENGINE_SMOKE,
					m_aWheelColPoints[0].point + 0.5f*GetUp(),
					1.2f*m_vecMoveSpeed, nil, 2.0f);

				CParticle::AddParticle(PARTICLE_ENGINE_STEAM,
					m_aWheelColPoints[2].point + 0.5f*GetUp(),
					1.3f*m_vecMoveSpeed, nil, 2.5f);
				CParticle::AddParticle(PARTICLE_ENGINE_SMOKE,
					m_aWheelColPoints[2].point + 0.5f*GetUp(),
					1.2f*m_vecMoveSpeed, nil, 2.0f);

				CParticle::AddParticle(PARTICLE_ENGINE_STEAM,
					m_aWheelColPoints[0].point + 0.5f*GetUp() - GetForward(),
					1.3f*m_vecMoveSpeed, nil, 2.5f);
				CParticle::AddParticle(PARTICLE_ENGINE_SMOKE,
					m_aWheelColPoints[0].point + 0.5f*GetUp() - GetForward(),
					1.2f*m_vecMoveSpeed, nil, 2.0f);

				CParticle::AddParticle(PARTICLE_ENGINE_STEAM,
					m_aWheelColPoints[2].point + 0.5f*GetUp() - GetForward(),
					1.3f*m_vecMoveSpeed, nil, 2.5f);
				CParticle::AddParticle(PARTICLE_ENGINE_SMOKE,
					m_aWheelColPoints[2].point + 0.5f*GetUp() - GetForward(),
					1.2f*m_vecMoveSpeed, nil, 2.0f);

				ApplyMoveForce(CVector(0.0f, 0.0f, 1.0f)*m_fMass*0.4f);
				ApplyTurnForce(GetUp()*m_fMass*0.035f, GetForward()*1.0f);
			}
		}
		break;
	}

	float brake;
	if(skipPhysics){
		bHasContacted = false;
		bIsInSafePosition = false;
		bWasPostponed = false;
		bHasHitWall = false;
		m_nCollisionRecords = 0;
		bHasCollided = false;
		bVehicleColProcessed = false;
		m_nDamagePieceType = 0;
		m_fDamageImpulse = 0.0f;
		m_pDamageEntity = nil;
		m_vecTurnFriction = CVector(0.0f, 0.0f, 0.0f);
		m_vecMoveFriction = CVector(0.0f, 0.0f, 0.0f);
	}else{

		// This has to be done if ProcessEntityCollision wasn't called
		if(!bVehicleColProcessed){
			CMatrix mat(GetMatrix());
			bIsStuck = false;
			bHasContacted = false;
			bIsInSafePosition = false;
			bWasPostponed = false;
			bHasHitWall = false;
			m_fDistanceTravelled = 0.0f;
			field_EF = false;
			m_phy_flagA80 = false;
			ApplyMoveSpeed();
			ApplyTurnSpeed();
			for(i = 0; CheckCollision() && i < 5; i++){
				GetMatrix() = mat;
				ApplyMoveSpeed();
				ApplyTurnSpeed();
			}
			bIsInSafePosition = true;
			bIsStuck = false;			
		}

		CPhysical::ProcessControl();

		ProcessBuoyancy();

		// Rescale spring ratios, i.e. subtract wheel radius
		for(i = 0; i < 4; i++){
			// wheel radius in relation to suspension line
			float wheelRadius = 1.0f - m_aSuspensionSpringLength[i]/m_aSuspensionLineLength[i];
			// rescale such that 0.0 is fully compressed and 1.0 is fully extended
			m_aSuspensionSpringRatio[i] = (m_aSuspensionSpringRatio[i]-wheelRadius)/(1.0f-wheelRadius);
		}

		float fwdSpeed = DotProduct(m_vecMoveSpeed, GetForward());
		CVector contactPoints[4];	// relative to model
		CVector contactSpeeds[4];	// speed at contact points
		CVector springDirections[4];	// normalized, in model space

		for(i = 0; i < 4; i++){
			// Set spring under certain circumstances
			if(Damage.GetWheelStatus(i) == WHEEL_STATUS_MISSING)
				m_aSuspensionSpringRatio[i] = 1.0f;
			else if(Damage.GetWheelStatus(i) == WHEEL_STATUS_BURST){
				// wheel more bumpy the faster we are
				if(CGeneral::GetRandomNumberInRange(0, (uint16)(40*fwdSpeed) + 98) < 100){
					m_aSuspensionSpringRatio[i] += 0.3f*(m_aSuspensionLineLength[i]-m_aSuspensionSpringLength[i])/m_aSuspensionSpringLength[i];
					if(m_aSuspensionSpringRatio[i] > 1.0f)
						m_aSuspensionSpringRatio[i] = 1.0f;
				}
			}

			// get points and directions if spring is compressed
			if(m_aSuspensionSpringRatio[i] < 1.0f){
				contactPoints[i] = m_aWheelColPoints[i].point - GetPosition();
				springDirections[i] = Multiply3x3(GetMatrix(), colModel->lines[i].p1 - colModel->lines[i].p0);
				springDirections[i].Normalise();
			}
		}

		// Make springs push up vehicle
		for(i = 0; i < 4; i++){
			if(m_aSuspensionSpringRatio[i] < 1.0f){
				float bias = pHandling->fSuspensionBias;
				if(i == 1 || i == 3)	// rear
					bias = 1.0f - bias;

				ApplySpringCollision(pHandling->fSuspensionForceLevel,
					springDirections[i], contactPoints[i],
					m_aSuspensionSpringRatio[i], bias);
				m_aWheelSkidmarkMuddy[i] =
					m_aWheelColPoints[i].surfaceB == SURFACE_GRASS ||
					m_aWheelColPoints[i].surfaceB == SURFACE_DIRTTRACK ||
					m_aWheelColPoints[i].surfaceB == SURFACE_SAND;
			}else{
				contactPoints[i] = Multiply3x3(GetMatrix(), colModel->lines[i].p1);
			}
		}

		// Get speed at contact points
		for(i = 0; i < 4; i++){
			contactSpeeds[i] = GetSpeed(contactPoints[i]);
			if(m_aGroundPhysical[i]){
				// subtract movement of physical we're standing on
				contactSpeeds[i] -= m_aGroundPhysical[i]->GetSpeed(m_aGroundOffset[i]);
#ifndef FIX_BUGS
				// this shouldn't be reset because we still need it below
				m_aGroundPhysical[i] = nil;
#endif
			}
		}

		// dampen springs
		for(i = 0; i < 4; i++)
			if(m_aSuspensionSpringRatio[i] < 1.0f)
				ApplySpringDampening(pHandling->fSuspensionDampingLevel,
					springDirections[i], contactPoints[i], contactSpeeds[i]);

		// Get speed at contact points again
		for(i = 0; i < 4; i++){
			contactSpeeds[i] = GetSpeed(contactPoints[i]);
			if(m_aGroundPhysical[i]){
				// subtract movement of physical we're standing on
				contactSpeeds[i] -= m_aGroundPhysical[i]->GetSpeed(m_aGroundOffset[i]);
				m_aGroundPhysical[i] = nil;
			}
		}


		bool gripCheat = true;
		fwdSpeed = DotProduct(m_vecMoveSpeed, GetForward());
		if(!strongGrip1 && !CVehicle::bCheat3)
			gripCheat = false;
		float acceleration = pHandling->Transmission.CalculateDriveAcceleration(m_fGasPedal, m_nCurrentGear, m_fChangeGearTime, fwdSpeed, gripCheat);
		acceleration /= fForceMultiplier;

		// unused
		if(GetModelIndex() == MI_MIAMI_RCBARON ||
		   GetModelIndex() == MI_MIAMI_RCRAIDER ||
		   GetModelIndex() == MI_MIAMI_SPARROW)
			acceleration = 0.0f;

		brake = m_fBrakePedal * pHandling->fBrakeDeceleration * CTimer::GetTimeStep();
		bool neutralHandling = !!(pHandling->Flags & HANDLING_NEUTRALHANDLING);
		float brakeBiasFront = neutralHandling ? 1.0f : 2.0f*pHandling->fBrakeBias;
		float brakeBiasRear  = neutralHandling ? 1.0f : 2.0f*(1.0f-pHandling->fBrakeBias);
		float tractionBiasFront = neutralHandling ? 1.0f : 2.0f*pHandling->fTractionBias;
		float tractionBiasRear  = neutralHandling ? 1.0f : 2.0f*(1.0f-pHandling->fTractionBias);

		// Count how many wheels are touching the ground

		m_nWheelsOnGround = 0;
		m_nDriveWheelsOnGroundPrev = m_nDriveWheelsOnGround;
		m_nDriveWheelsOnGround = 0;

		for(i = 0; i < 4; i++){
			if(m_aSuspensionSpringRatio[i] < 1.0f)
				m_aWheelTimer[i] = 4.0f;
			else
				m_aWheelTimer[i] = max(m_aWheelTimer[i]-CTimer::GetTimeStep(), 0.0f);

			if(m_aWheelTimer[i] > 0.0f){
				m_nWheelsOnGround++;
				switch(pHandling->Transmission.nDriveType){
				case '4':
					m_nDriveWheelsOnGround++;
					break;
				case 'F':
					if(i == CARWHEEL_FRONT_LEFT || i == CARWHEEL_FRONT_RIGHT)
						m_nDriveWheelsOnGround++;
					break;
				case 'R':
					if(i == CARWHEEL_REAR_LEFT || i == CARWHEEL_REAR_RIGHT)
						m_nDriveWheelsOnGround++;
					break;
				}
			}
		}

		float traction;
		if(m_status == STATUS_PHYSICS)
			traction = 0.004f * m_fTraction;
		else
			traction = 0.004f;
		traction *= pHandling->fTractionMultiplier / 4.0f;
		traction /= fForceMultiplier;
		if(CVehicle::bCheat3)
			traction *= 4.0f;

		if(FindPlayerVehicle() && FindPlayerVehicle() == this){
			if(CPad::GetPad(0)->WeaponJustDown()){
				if(m_bombType == CARBOMB_TIMED){
					m_bombType = CARBOMB_TIMEDACTIVE;
					m_nBombTimer = 7000;
					m_pBlowUpEntity = FindPlayerPed();
					CGarages::TriggerMessage("GA_12", -1, 3000, -1);
					DMAudio.PlayOneShot(m_audioEntityId, SOUND_BOMB_TIMED_ACTIVATED, 1.0f);
				}else if(m_bombType == CARBOMB_ONIGNITION){
					m_bombType = CARBOMB_ONIGNITIONACTIVE;
					CGarages::TriggerMessage("GA_12", -1, 3000, -1);
					DMAudio.PlayOneShot(m_audioEntityId, SOUND_BOMB_ONIGNITION_ACTIVATED, 1.0f);
				}
			}
		}else if(strongGrip1 || CVehicle::bCheat3){
			traction *= 1.2f;
			acceleration *= 1.4f;
			if(strongGrip2 || CVehicle::bCheat3){
				traction *= 1.3f;
				acceleration *= 1.4f;
			}
		}

		static float fThrust;
		static tWheelState WheelState[4];

		// Process front wheels on ground

		if(m_aWheelTimer[CARWHEEL_FRONT_LEFT] > 0.0f || m_aWheelTimer[CARWHEEL_FRONT_RIGHT] > 0.0f){
			float s = Sin(m_fSteerAngle);
			float c = Cos(m_fSteerAngle);
			CVector wheelFwd = Multiply3x3(GetMatrix(), CVector(-s, c, 0.0f));
			CVector wheelRight = Multiply3x3(GetMatrix(), CVector(c, s, 0.0f));

			if(m_aWheelTimer[CARWHEEL_FRONT_LEFT] > 0.0f){
				if(mod_HandlingManager.HasRearWheelDrive(pHandling->nIdentifier))
					fThrust = 0.0f;
				else
					fThrust = acceleration;

				m_aWheelColPoints[CARWHEEL_FRONT_LEFT].surfaceA = SURFACE_RUBBER29;
				float adhesion = CSurfaceTable::GetAdhesiveLimit(m_aWheelColPoints[CARWHEEL_FRONT_LEFT])*traction;
				if(m_status == STATUS_PLAYER)
					adhesion *= CSurfaceTable::GetWetMultiplier(m_aWheelColPoints[CARWHEEL_FRONT_LEFT].surfaceB);
				WheelState[CARWHEEL_FRONT_LEFT] = m_aWheelState[CARWHEEL_FRONT_LEFT];

				if(Damage.GetWheelStatus(VEHWHEEL_FRONT_LEFT) == WHEEL_STATUS_BURST)
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_FRONT_LEFT], contactPoints[CARWHEEL_FRONT_LEFT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasFront,
						adhesion*tractionBiasFront*Damage.m_fWheelDamageEffect,
						CARWHEEL_FRONT_LEFT,
						&m_aWheelSpeed[CARWHEEL_FRONT_LEFT],
						&WheelState[CARWHEEL_FRONT_LEFT],
						WHEEL_STATUS_BURST);
				else
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_FRONT_LEFT], contactPoints[CARWHEEL_FRONT_LEFT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasFront,
						adhesion*tractionBiasFront,
						CARWHEEL_FRONT_LEFT,
						&m_aWheelSpeed[CARWHEEL_FRONT_LEFT],
						&WheelState[CARWHEEL_FRONT_LEFT],
						WHEEL_STATUS_OK);
			}

			if(m_aWheelTimer[CARWHEEL_FRONT_RIGHT] > 0.0f){
				if(mod_HandlingManager.HasRearWheelDrive(pHandling->nIdentifier))
					fThrust = 0.0f;
				else
					fThrust = acceleration;

				m_aWheelColPoints[CARWHEEL_FRONT_RIGHT].surfaceA = SURFACE_RUBBER29;
				float adhesion = CSurfaceTable::GetAdhesiveLimit(m_aWheelColPoints[CARWHEEL_FRONT_RIGHT])*traction;
				if(m_status == STATUS_PLAYER)
					adhesion *= CSurfaceTable::GetWetMultiplier(m_aWheelColPoints[CARWHEEL_FRONT_RIGHT].surfaceB);
				WheelState[CARWHEEL_FRONT_RIGHT] = m_aWheelState[CARWHEEL_FRONT_RIGHT];

				if(Damage.GetWheelStatus(VEHWHEEL_FRONT_RIGHT) == WHEEL_STATUS_BURST)
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_FRONT_RIGHT], contactPoints[CARWHEEL_FRONT_RIGHT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasFront,
						adhesion*tractionBiasFront*Damage.m_fWheelDamageEffect,
						CARWHEEL_FRONT_RIGHT,
						&m_aWheelSpeed[CARWHEEL_FRONT_RIGHT],
						&WheelState[CARWHEEL_FRONT_RIGHT],
						WHEEL_STATUS_BURST);
				else
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_FRONT_RIGHT], contactPoints[CARWHEEL_FRONT_RIGHT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasFront,
						adhesion*tractionBiasFront,
						CARWHEEL_FRONT_RIGHT,
						&m_aWheelSpeed[CARWHEEL_FRONT_RIGHT],
						&WheelState[CARWHEEL_FRONT_RIGHT],
						WHEEL_STATUS_OK);
			}
		}

		// Process front wheels off ground

		if(m_aWheelTimer[CARWHEEL_FRONT_LEFT] <= 0.0f){
			if(mod_HandlingManager.HasRearWheelDrive(pHandling->nIdentifier) || acceleration == 0.0f)
				m_aWheelSpeed[CARWHEEL_FRONT_LEFT] *= 0.95f;
			else{
				if(acceleration > 0.0f){
					if(m_aWheelSpeed[CARWHEEL_FRONT_LEFT] < 2.0f)
						m_aWheelSpeed[CARWHEEL_FRONT_LEFT] -= 0.2f;
				}else{
					if(m_aWheelSpeed[CARWHEEL_FRONT_LEFT] > -2.0f)
						m_aWheelSpeed[CARWHEEL_FRONT_LEFT] += 0.1f;
				}
			}
			m_aWheelRotation[CARWHEEL_FRONT_LEFT] += m_aWheelSpeed[CARWHEEL_FRONT_LEFT];
		}
		if(m_aWheelTimer[CARWHEEL_FRONT_RIGHT] <= 0.0f){
			if(mod_HandlingManager.HasRearWheelDrive(pHandling->nIdentifier) || acceleration == 0.0f)
				m_aWheelSpeed[CARWHEEL_FRONT_RIGHT] *= 0.95f;
			else{
				if(acceleration > 0.0f){
					if(m_aWheelSpeed[CARWHEEL_FRONT_RIGHT] < 2.0f)
						m_aWheelSpeed[CARWHEEL_FRONT_RIGHT] -= 0.2f;
				}else{
					if(m_aWheelSpeed[CARWHEEL_FRONT_RIGHT] > -2.0f)
						m_aWheelSpeed[CARWHEEL_FRONT_RIGHT] += 0.1f;
				}
			}
			m_aWheelRotation[CARWHEEL_FRONT_RIGHT] += m_aWheelSpeed[CARWHEEL_FRONT_RIGHT];
		}

		// Process rear wheels

		if(m_aWheelTimer[CARWHEEL_REAR_LEFT] > 0.0f || m_aWheelTimer[CARWHEEL_REAR_RIGHT] > 0.0f){
			CVector wheelFwd = GetForward();
			CVector wheelRight = GetRight();

			if(bIsHandbrakeOn)
				brake = 20000.0f;

			if(m_aWheelTimer[CARWHEEL_REAR_LEFT] > 0.0f){
				if(mod_HandlingManager.HasFrontWheelDrive(pHandling->nIdentifier))
					fThrust = 0.0f;
				else
					fThrust = acceleration;

				m_aWheelColPoints[CARWHEEL_REAR_LEFT].surfaceA = SURFACE_RUBBER29;
				float adhesion = CSurfaceTable::GetAdhesiveLimit(m_aWheelColPoints[CARWHEEL_REAR_LEFT])*traction;
				if(m_status == STATUS_PLAYER)
					adhesion *= CSurfaceTable::GetWetMultiplier(m_aWheelColPoints[CARWHEEL_REAR_LEFT].surfaceB);
				WheelState[CARWHEEL_REAR_LEFT] = m_aWheelState[CARWHEEL_REAR_LEFT];

				if(Damage.GetWheelStatus(VEHWHEEL_REAR_LEFT) == WHEEL_STATUS_BURST)
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_REAR_LEFT], contactPoints[CARWHEEL_REAR_LEFT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasRear,
						adhesion*tractionBiasRear*Damage.m_fWheelDamageEffect,
						CARWHEEL_REAR_LEFT,
						&m_aWheelSpeed[CARWHEEL_REAR_LEFT],
						&WheelState[CARWHEEL_REAR_LEFT],
						WHEEL_STATUS_BURST);
				else
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_REAR_LEFT], contactPoints[CARWHEEL_REAR_LEFT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasRear,
						adhesion*tractionBiasRear,
						CARWHEEL_REAR_LEFT,
						&m_aWheelSpeed[CARWHEEL_REAR_LEFT],
						&WheelState[CARWHEEL_REAR_LEFT],
						WHEEL_STATUS_OK);
			}

			if(m_aWheelTimer[CARWHEEL_REAR_RIGHT] > 0.0f){
				if(mod_HandlingManager.HasFrontWheelDrive(pHandling->nIdentifier))
					fThrust = 0.0f;
				else
					fThrust = acceleration;

				m_aWheelColPoints[CARWHEEL_REAR_RIGHT].surfaceA = SURFACE_RUBBER29;
				float adhesion = CSurfaceTable::GetAdhesiveLimit(m_aWheelColPoints[CARWHEEL_REAR_RIGHT])*traction;
				if(m_status == STATUS_PLAYER)
					adhesion *= CSurfaceTable::GetWetMultiplier(m_aWheelColPoints[CARWHEEL_REAR_RIGHT].surfaceB);
				WheelState[CARWHEEL_REAR_RIGHT] = m_aWheelState[CARWHEEL_REAR_RIGHT];

				if(Damage.GetWheelStatus(VEHWHEEL_REAR_RIGHT) == WHEEL_STATUS_BURST)
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_REAR_RIGHT], contactPoints[CARWHEEL_REAR_RIGHT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasRear,
						adhesion*tractionBiasRear*Damage.m_fWheelDamageEffect,
						CARWHEEL_REAR_RIGHT,
						&m_aWheelSpeed[CARWHEEL_REAR_RIGHT],
						&WheelState[CARWHEEL_REAR_RIGHT],
						WHEEL_STATUS_BURST);
				else
					ProcessWheel(wheelFwd, wheelRight,
						contactSpeeds[CARWHEEL_REAR_RIGHT], contactPoints[CARWHEEL_REAR_RIGHT],
						m_nWheelsOnGround, fThrust,
						brake*brakeBiasRear,
						adhesion*tractionBiasRear,
						CARWHEEL_REAR_RIGHT,
						&m_aWheelSpeed[CARWHEEL_REAR_RIGHT],
						&WheelState[CARWHEEL_REAR_RIGHT],
						WHEEL_STATUS_OK);
			}
		}

		// Process rear wheels off ground

		if(m_aWheelTimer[CARWHEEL_REAR_LEFT] <= 0.0f){
			if(mod_HandlingManager.HasFrontWheelDrive(pHandling->nIdentifier) || acceleration == 0.0f)
				m_aWheelSpeed[CARWHEEL_REAR_LEFT] *= 0.95f;
			else{
				if(acceleration > 0.0f){
					if(m_aWheelSpeed[CARWHEEL_REAR_LEFT] < 2.0f)
						m_aWheelSpeed[CARWHEEL_REAR_LEFT] -= 0.2f;
				}else{
					if(m_aWheelSpeed[CARWHEEL_REAR_LEFT] > -2.0f)
						m_aWheelSpeed[CARWHEEL_REAR_LEFT] += 0.1f;
				}
			}
			m_aWheelRotation[CARWHEEL_REAR_LEFT] += m_aWheelSpeed[CARWHEEL_REAR_LEFT];
		}
		if(m_aWheelTimer[CARWHEEL_REAR_RIGHT] <= 0.0f){
			if(mod_HandlingManager.HasFrontWheelDrive(pHandling->nIdentifier) || acceleration == 0.0f)
				m_aWheelSpeed[CARWHEEL_REAR_RIGHT] *= 0.95f;
			else{
				if(acceleration > 0.0f){
					if(m_aWheelSpeed[CARWHEEL_REAR_RIGHT] < 2.0f)
						m_aWheelSpeed[CARWHEEL_REAR_RIGHT] -= 0.2f;
				}else{
					if(m_aWheelSpeed[CARWHEEL_REAR_RIGHT] > -2.0f)
						m_aWheelSpeed[CARWHEEL_REAR_RIGHT] += 0.1f;
				}
			}
			m_aWheelRotation[CARWHEEL_REAR_RIGHT] += m_aWheelSpeed[CARWHEEL_REAR_RIGHT];
		}

		for(i = 0; i < 4; i++){
			float wheelPos = colModel->lines[i].p0.z;
			if(m_aSuspensionSpringRatio[i] > 0.0f)
				wheelPos -= m_aSuspensionSpringRatio[i]*m_aSuspensionSpringLength[i];
			m_aWheelPosition[i] += (wheelPos - m_aWheelPosition[i])*0.75f;
		}
		for(i = 0; i < 4; i++)
			m_aWheelState[i] = WheelState[i];

		// Process horn

		if(m_status != STATUS_PLAYER){
			ReduceHornCounter();
		}else{
			if(GetModelIndex() == MI_MRWHOOP){
				if(Pads[0].bHornHistory[Pads[0].iCurrHornHistory] &&
				   !Pads[0].bHornHistory[(Pads[0].iCurrHornHistory+4) % 5]){
					m_bSirenOrAlarm = !m_bSirenOrAlarm;
					printf("m_bSirenOrAlarm toggled to %d\n", m_bSirenOrAlarm);
				}
			}else if(UsesSiren(GetModelIndex())){
				if(Pads[0].bHornHistory[Pads[0].iCurrHornHistory]){
					if(Pads[0].bHornHistory[(Pads[0].iCurrHornHistory+4) % 5] &&
					   Pads[0].bHornHistory[(Pads[0].iCurrHornHistory+3) % 5])
						m_nCarHornTimer = 1;
					else
						m_nCarHornTimer = 0;
				}else if(Pads[0].bHornHistory[(Pads[0].iCurrHornHistory+4) % 5] &&
				         !Pads[0].bHornHistory[(Pads[0].iCurrHornHistory+1) % 5]){
					m_nCarHornTimer = 0;
					m_bSirenOrAlarm = !m_bSirenOrAlarm;
				}else
					m_nCarHornTimer = 0;
			}else if(GetModelIndex() != MI_YARDIE && !CVehicle::bCheat3){
				if(Pads[0].GetHorn())
					m_nCarHornTimer = 1;
				else
					m_nCarHornTimer = 0;
			}
		}

		// Flying

		if(m_status != STATUS_PLAYER && m_status != STATUS_PLAYER_REMOTE && m_status != STATUS_PHYSICS){
			if(GetModelIndex() == MI_MIAMI_RCRAIDER || GetModelIndex() == MI_MIAMI_SPARROW)
				m_aWheelSpeed[0] = max(m_aWheelSpeed[0]-0.0005f, 0.0f);
		}else if((GetModelIndex() == MI_DODO || CVehicle::bAllDodosCheat) &&
		         m_vecMoveSpeed.Magnitude() > 0.0f && CTimer::GetTimeStep() > 0.0f){
			FlyingControl(FLIGHT_MODEL_DODO);
		}else if(GetModelIndex() == MI_MIAMI_RCBARON){
			FlyingControl(FLIGHT_MODEL_HELI);
		}else if(GetModelIndex() == MI_MIAMI_RCRAIDER || GetModelIndex() == MI_MIAMI_SPARROW || bAllCarCheat){
			if(CPad::GetPad(0)->GetCircleJustDown())
				m_aWheelSpeed[0] = max(m_aWheelSpeed[0]-0.03f, 0.0f);
			if(m_aWheelSpeed[0] < 0.22f)
				m_aWheelSpeed[0] += 0.0001f;
			if(m_aWheelSpeed[0] > 0.15f)
				FlyingControl(FLIGHT_MODEL_HELI);
		}
	}



	// Process car on fire
	// A similar calculation of damagePos is done elsewhere for smoke

	uint8 engineStatus = Damage.GetEngineStatus();
	CVector damagePos = ((CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex()))->m_positions[CAR_POS_HEADLIGHTS];

	switch(Damage.GetDoorStatus(DOOR_BONNET)){
	case DOOR_STATUS_OK:
	case DOOR_STATUS_SMASHED:
		// Bonnet is still there, smoke comes out at the edge
		damagePos += vecDAMAGE_ENGINE_POS_SMALL;
		break;
	case DOOR_STATUS_SWINGING:
	case DOOR_STATUS_MISSING:
		// Bonnet is gone, smoke comes out at the engine
		damagePos += vecDAMAGE_ENGINE_POS_BIG;
		break;
	}

	// move fire forward if in first person
	if(this == FindPlayerVehicle() && TheCamera.GetLookingForwardFirstPerson())
		if(m_fHealth < 250.0f && m_status != STATUS_WRECKED){
			if(GetModelIndex() == MI_FIRETRUCK)
				damagePos += CVector(0.0f, 3.0f, -0.2f);
			else
				damagePos += CVector(0.0f, 1.2f, -0.8f);
		}

	damagePos = GetMatrix()*damagePos;
	damagePos.z += 0.15f;

	if(m_fHealth < 250.0f && m_status != STATUS_WRECKED){
		// Car is on fire

		CParticle::AddParticle(PARTICLE_CARFLAME, damagePos,
			CVector(0.0f, 0.0f, CGeneral::GetRandomNumberInRange(0.01125f, 0.09f)),
			nil, 0.9f);

		CVector coors = damagePos;
		coors.x += CGeneral::GetRandomNumberInRange(-0.5625f, 0.5625f),
		coors.y += CGeneral::GetRandomNumberInRange(-0.5625f, 0.5625f),
		coors.z += CGeneral::GetRandomNumberInRange(0.5625f, 2.25f);
		CParticle::AddParticle(PARTICLE_CARFLAME_SMOKE, coors, CVector(0.0f, 0.0f, 0.0f));

		CParticle::AddParticle(PARTICLE_ENGINE_SMOKE2, damagePos, CVector(0.0f, 0.0f, 0.0f), nil, 0.5f);

		// Blow up car after 5 seconds
		m_fFireBlowUpTimer += CTimer::GetTimeStepInMilliseconds();
		if(m_fFireBlowUpTimer > 5000.0f){
			CWorld::Players[CWorld::PlayerInFocus].AwardMoneyForExplosion(this);
			BlowUpCar(m_pSetOnFireEntity);
		}
	}else
		m_fFireBlowUpTimer = 0.0f;

	// Decrease car health if engine is damaged badly
	if(engineStatus > ENGINE_STATUS_ON_FIRE && m_fHealth > 250.0f)
		m_fHealth -= 2.0f;

	ProcessDelayedExplosion();


	if(m_bSirenOrAlarm && (CTimer::GetFrameCounter()&7) == 5 &&
	   UsesSiren(GetModelIndex()) && GetModelIndex() != MI_RCBANDIT)
		CCarAI::MakeWayForCarWithSiren(this);


	// Find out how much to shake the pad depending on suspension and ground surface

	float suspShake = 0.0f;
	float surfShake = 0.0f;
	for(i = 0; i < 4; i++){
		float suspChange = m_aSuspensionSpringRatioPrev[i] - m_aSuspensionSpringRatio[i];
		if(suspChange > 0.3f){
			DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_JUMP, suspChange);
			if(suspChange > suspShake)
				suspShake = suspChange;
		}

		uint8 surf = m_aWheelColPoints[i].surfaceB;
		if(surf == SURFACE_DIRT || surf == SURFACE_PUDDLE || surf == SURFACE_HEDGE){
			if(surfShake < 0.2f)
				surfShake = 0.3f;
		}else if(surf == SURFACE_DIRTTRACK || surf == SURFACE_SAND){
			if(surfShake < 0.1f)
				surfShake = 0.2f;
		}else if(surf == SURFACE_GRASS){
			if(surfShake < 0.05f)
				surfShake = 0.1f;
		}

		m_aSuspensionSpringRatioPrev[i] = m_aSuspensionSpringRatio[i];
		m_aSuspensionSpringRatio[i] = 1.0f;
	}

	// Shake pad

	if((suspShake > 0.0f || surfShake > 0.0f) && m_status == STATUS_PLAYER){
		float speed = m_vecMoveSpeed.MagnitudeSqr();
		if(speed > sq(0.1f)){
			speed = Sqrt(speed);
			if(suspShake > 0.0f){
				uint8 freq = min(200.0f*suspShake*speed*2000.0f/m_fMass + 100.0f, 250.0f);
				CPad::GetPad(0)->StartShake(20000.0f*CTimer::GetTimeStep()/freq, freq);
			}else{
				uint8 freq = min(200.0f*surfShake*speed*2000.0f/m_fMass + 40.0f, 145.0f);
				CPad::GetPad(0)->StartShake(5000.0f*CTimer::GetTimeStep()/freq, freq);
			}
		}
	}

	bVehicleColProcessed = false;

	if(!bWarnedPeds)
		CCarCtrl::ScanForPedDanger(this);


	// Turn around at the edge of the world
	// TODO: make the numbers defines

	float heading;
	if(GetPosition().x > 1900.0f){
		if(m_vecMoveSpeed.x > 0.0f)
			m_vecMoveSpeed.x *= -1.0f;
		heading = GetForward().Heading();
		if(heading > 0.0f)	// going west
			SetHeading(-heading);
	}else if(GetPosition().x < -1900.0f){
		if(m_vecMoveSpeed.x < 0.0f)
			m_vecMoveSpeed.x *= -1.0f;
		heading = GetForward().Heading();
		if(heading < 0.0f)	// going east
			SetHeading(-heading);
	}
	if(GetPosition().y > 1900.0f){
		if(m_vecMoveSpeed.y > 0.0f)
			m_vecMoveSpeed.y *= -1.0f;
		heading = GetForward().Heading();
		if(heading < HALFPI && heading > 0.0f)
			SetHeading(PI-heading);
		else if(heading > -HALFPI && heading < 0.0f)
			SetHeading(-PI-heading);
	}else if(GetPosition().y < -1900.0f){
		if(m_vecMoveSpeed.y < 0.0f)
			m_vecMoveSpeed.y *= -1.0f;
		heading = GetForward().Heading();
		if(heading > HALFPI)
			SetHeading(PI-heading);
		else if(heading < -HALFPI)
			SetHeading(-PI-heading);
	}

	if(bInfiniteMass){
		m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
		m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
		m_vecMoveFriction = CVector(0.0f, 0.0f, 0.0f);
		m_vecTurnFriction = CVector(0.0f, 0.0f, 0.0f);
	}else if(!skipPhysics &&
	         (m_fGasPedal == 0.0f && brake == 0.0f || m_status == STATUS_WRECKED)){
		if(Abs(m_vecMoveSpeed.x) < 0.005f &&
		   Abs(m_vecMoveSpeed.y) < 0.005f &&
		   Abs(m_vecMoveSpeed.z) < 0.005f){
			m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
			m_vecTurnSpeed.z = 0.0f;
		}
	}
}

void
CAutomobile::Teleport(CVector pos)
{
	CWorld::Remove(this);

	GetPosition() = pos;
	SetOrientation(0.0f, 0.0f, 0.0f);
	SetMoveSpeed(0.0f, 0.0f, 0.0f);
	SetTurnSpeed(0.0f, 0.0f, 0.0f);

	ResetSuspension();

	CWorld::Add(this);
}

WRAPPER void CAutomobile::PreRender(void) { EAXJMP(0x535B40); }
WRAPPER void CAutomobile::Render(void) { EAXJMP(0x539EA0); }


int32
CAutomobile::ProcessEntityCollision(CEntity *ent, CColPoint *colpoints)
{
	int i;
	CColModel *colModel;

	if(m_status != STATUS_SIMPLE)
		bVehicleColProcessed = true;

	if(m_veh_flagC80)
		colModel = &CWorld::Players[CWorld::PlayerInFocus].m_ColModel;
	else
		colModel = GetColModel();

	int numWheelCollisions = 0;
	float prevRatios[4] = { 0.0f, 0.0f, 0.0f, 0.0f};
	for(i = 0; i < 4; i++)
		prevRatios[i] = m_aSuspensionSpringRatio[i];

	int numCollisions = CCollision::ProcessColModels(GetMatrix(), *colModel,
		ent->GetMatrix(), *ent->GetColModel(),
		colpoints,
		m_aWheelColPoints, m_aSuspensionSpringRatio);

	// m_aSuspensionSpringRatio are now set to the point where the tyre touches ground.
	// In ProcessControl these will be re-normalized to ignore the tyre radius.

	if(field_EF || m_phy_flagA80 ||
	   GetModelIndex() == MI_DODO && (ent->IsPed() || ent->IsVehicle())){
		// don't do line collision
		for(i = 0; i < 4; i++)
			m_aSuspensionSpringRatio[i] = prevRatios[i];
	}else{
		for(i = 0; i < 4; i++)
			if(m_aSuspensionSpringRatio[i] < 1.0f && m_aSuspensionSpringRatio[i] < prevRatios[i]){
				numWheelCollisions++;

				// wheel is touching a physical
				if(ent->IsVehicle() || ent->IsObject()){
					CPhysical *phys = (CPhysical*)ent;

					m_aGroundPhysical[i] = phys;
					phys->RegisterReference((CEntity**)&m_aGroundPhysical[i]);
					m_aGroundOffset[i] = m_aWheelColPoints[i].point - phys->GetPosition();

					if(phys->GetModelIndex() == MI_BODYCAST && m_status == STATUS_PLAYER){
						// damage body cast
						float speed = m_vecMoveSpeed.MagnitudeSqr();
						if(speed > 0.1f){
							CObject::nBodyCastHealth -= 0.1f*m_fMass*speed;
							DMAudio.PlayOneShot(m_audioEntityId, SOUND_PED_BODYCAST_HIT, 0.0f);
						}

						// move body cast
						if(phys->bIsStatic){
							phys->bIsStatic = false;
							phys->m_nStaticFrames = 0;
							phys->ApplyMoveForce(m_vecMoveSpeed / speed);
							phys->AddToMovingList();
						}
					}
				}

				m_nSurfaceTouched = m_aWheelColPoints[i].surfaceB;
				if(ent->IsBuilding())
					m_pCurGroundEntity = ent;
			}
	}

	if(numCollisions > 0 || numWheelCollisions > 0){
		AddCollisionRecord(ent);
		if(!ent->IsBuilding())
			((CPhysical*)ent)->AddCollisionRecord(this);

		if(numCollisions > 0)
			if(ent->IsBuilding() ||
			   ent->IsObject() && ((CPhysical*)ent)->bInfiniteMass)
				bHasHitWall = true;
	}

	return numCollisions;
}

static int16 nLastControlInput;
static float fMouseCentreRange = 0.35f;
static float fMouseSteerSens = -0.0035f;
static float fMouseCentreMult = 0.975f;

void
CAutomobile::ProcessControlInputs(uint8 pad)
{
	float speed = DotProduct(m_vecMoveSpeed, GetForward());

	if(CPad::GetPad(pad)->GetExitVehicle())
		bIsHandbrakeOn = true;
	else
		bIsHandbrakeOn = !!CPad::GetPad(pad)->GetHandBrake();

	// Steer left/right
	if(CCamera::m_bUseMouse3rdPerson && !CVehicle::m_bDisableMouseSteering){
		if(CPad::GetPad(pad)->GetMouseX() != 0.0f){
			m_fSteerRatio += fMouseSteerSens*CPad::GetPad(pad)->GetMouseX();
			nLastControlInput = 2;
			if(Abs(m_fSteerRatio) < fMouseCentreRange)
				m_fSteerRatio *= Pow(fMouseCentreMult, CTimer::GetTimeStep());
		}else if(CPad::GetPad(pad)->GetSteeringLeftRight() || nLastControlInput != 2){
			// mouse hasn't move, steer with pad like below
			m_fSteerRatio += (-CPad::GetPad(pad)->GetSteeringLeftRight()/128.0f - m_fSteerRatio)*
				0.2f*CTimer::GetTimeStep();
			nLastControlInput = 0;
		}
	}else{
		m_fSteerRatio += (-CPad::GetPad(pad)->GetSteeringLeftRight()/128.0f - m_fSteerRatio)*
			0.2f*CTimer::GetTimeStep();
		nLastControlInput = 0;
	}
	m_fSteerRatio = clamp(m_fSteerRatio, -1.0f, 1.0f);

	// Accelerate/Brake
	float acceleration = (CPad::GetPad(pad)->GetAccelerate() - CPad::GetPad(pad)->GetBrake())/255.0f;
	if(GetModelIndex() == MI_DODO && acceleration < 0.0f)
		acceleration *= 0.3f;
	if(Abs(speed) < 0.01f){
		// standing still, go into direction we want
		m_fGasPedal = acceleration;
		m_fBrakePedal = 0.0f;
	}else{
#if 1
		// simpler than the code below
		if(speed * acceleration < 0.0f){
			// if opposite directions, have to brake first
			m_fGasPedal = 0.0f;
			m_fBrakePedal = Abs(acceleration);
		}else{
			// accelerating in same direction we were already going
			m_fGasPedal = acceleration;
			m_fBrakePedal = 0.0f;
		}
#else
		if(speed < 0.0f){
			// moving backwards currently
			if(acceleration < 0.0f){
				// still go backwards
				m_fGasPedal = acceleration;
				m_fBrakePedal = 0.0f;
			}else{
				// want to go forwards, so brake
				m_fGasPedal = 0.0f;
				m_fBrakePedal = acceleration;
			}
		}else{
			// moving forwards currently
			if(acceleration < 0.0f){
				// want to go backwards, so brake
				m_fGasPedal = 0.0f;
				m_fBrakePedal = -acceleration;
			}else{
				// still go forwards
				m_fGasPedal = acceleration;
				m_fBrakePedal = 0.0f;
			}
		}
#endif
	}

	// Actually turn wheels
	static float fValue;	// why static?
	if(m_fSteerRatio < 0.0f)
		fValue = -sq(m_fSteerRatio);
	else
		fValue = sq(m_fSteerRatio);
	m_fSteerAngle = DEGTORAD(pHandling->fSteeringLock) * fValue;

	if(bComedyControls){
		int rnd = CGeneral::GetRandomNumber() % 10;
		switch(m_comedyControlState){
		case 0:
			if(rnd < 2)
				m_comedyControlState = 1;
			else if(rnd < 4)
				m_comedyControlState = 2;
			break;
		case 1:
			m_fSteerAngle += 0.05f;
			if(rnd < 2)
				m_comedyControlState = 0;
			break;
		case 2:
			m_fSteerAngle -= 0.05f;
			if(rnd < 2)
				m_comedyControlState = 0;
			break;
		}
	}else
		m_comedyControlState = 0;

	// Brake if player isn't in control
	// BUG: game always uses pad 0 here
	if(CPad::GetPad(pad)->DisablePlayerControls){
		m_fBrakePedal = 1.0f;
		bIsHandbrakeOn = true;
		m_fGasPedal = 0.0f;

		FindPlayerPed()->KeepAreaAroundPlayerClear();

		// slow down car immediately
		speed = m_vecMoveSpeed.Magnitude();
		if(speed > 0.28f)
			m_vecMoveSpeed *= 0.28f/speed;
	}
}

WRAPPER void
CAutomobile::FireTruckControl(void)
{ EAXJMP(0x522590);
}

WRAPPER void
CAutomobile::TankControl(void)
{ EAXJMP(0x53D530);
}

WRAPPER void
CAutomobile::HydraulicControl(void)
{ EAXJMP(0x52D4E0);
}

WRAPPER void
CAutomobile::ProcessBuoyancy(void)
{ EAXJMP(0x5308D0);
}

WRAPPER void
CAutomobile::DoDriveByShootings(void)
{ EAXJMP(0x564000);
}

WRAPPER int32
CAutomobile::RcbanditCheckHitWheels(void)
{ EAXJMP(0x53C990);
}

#if 0
WRAPPER void
CAutomobile::VehicleDamage(float impulse, uint16 damagedPiece)
{ EAXJMP(0x52F390); }
#else
void
CAutomobile::VehicleDamage(float impulse, uint16 damagedPiece)
{
	int i;
	float damageMultiplier = 0.2f;
	bool doubleMoney = false;

	if(impulse == 0.0f){
		impulse = m_fDamageImpulse;
		damagedPiece = m_nDamagePieceType;
		damageMultiplier = 1.0f;
	}

	CVector pos(0.0f, 0.0f, 0.0f);

	if(!bCanBeDamaged)
		return;

	// damage flipped over car
	if(GetUp().z < 0.0f && this != FindPlayerVehicle()){
		if(bNotDamagedUpsideDown || m_status == STATUS_PLAYER_REMOTE || bIsInWater)
			return;
		m_fHealth -= 4.0f*CTimer::GetTimeStep();
	}

	if(impulse > 25.0f && m_status != STATUS_WRECKED){
		if(bIsLawEnforcer &&
		   FindPlayerVehicle() && FindPlayerVehicle() == m_pDamageEntity &&
		   m_status != STATUS_ABANDONED &&
		   FindPlayerVehicle()->m_vecMoveSpeed.Magnitude() >= m_vecMoveSpeed.Magnitude() &&
		   FindPlayerVehicle()->m_vecMoveSpeed.Magnitude() > 0.1f)
			FindPlayerPed()->SetWantedLevelNoDrop(1);

		if(m_status == STATUS_PLAYER && impulse > 50.0f){
			uint8 freq = min(0.4f*impulse*2000.0f/m_fMass + 100.0f, 250.0f);
			CPad::GetPad(0)->StartShake(40000/freq, freq);
		}

		if(bOnlyDamagedByPlayer){
			if(m_pDamageEntity != FindPlayerPed() &&
			   m_pDamageEntity != FindPlayerVehicle())
				return;
		}

		if(bCollisionProof)
			return;

		if(m_pDamageEntity){
			if(m_pDamageEntity->IsBuilding() &&
			   DotProduct(m_vecDamageNormal, GetUp()) > 0.6f)
				return;
		}

		int oldLightStatus[4];
		for(i = 0; i < 4; i++)
			oldLightStatus[i] = Damage.GetLightStatus((eLights)i);

		if(GetUp().z > 0.0f || m_vecMoveSpeed.MagnitudeSqr() > 0.1f){
			float impulseMult = bMoreResistantToDamage ? 0.5f : 4.0f;

			switch(damagedPiece){
			case CAR_PIECE_BUMP_FRONT:
				GetComponentWorldPosition(CAR_BUMP_FRONT, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if(Damage.ApplyDamage(COMPONENT_BUMPER_FRONT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetBumperDamage(CAR_BUMP_FRONT, VEHBUMPER_FRONT);
					doubleMoney = true;
				}
				if(m_aCarNodes[CAR_BONNET] && Damage.GetPanelStatus(VEHBUMPER_FRONT) == PANEL_STATUS_MISSING){
			case CAR_PIECE_BONNET:
					GetComponentWorldPosition(CAR_BONNET, pos);
					dmgDrawCarCollidingParticles(pos, impulse);
					if(Damage.ApplyDamage(COMPONENT_DOOR_BONNET, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
						SetDoorDamage(CAR_BONNET, DOOR_BONNET);
						doubleMoney = true;
					}
				}
				break;

			case CAR_PIECE_BUMP_REAR:
				GetComponentWorldPosition(CAR_BUMP_REAR, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if(Damage.ApplyDamage(COMPONENT_BUMPER_FRONT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetBumperDamage(CAR_BUMP_REAR, VEHBUMPER_REAR);
					doubleMoney = true;
				}
				if(m_aCarNodes[CAR_BOOT] && Damage.GetPanelStatus(VEHBUMPER_REAR) == PANEL_STATUS_MISSING){
			case CAR_PIECE_BOOT:
					GetComponentWorldPosition(CAR_BOOT, pos);
					dmgDrawCarCollidingParticles(pos, impulse);
					if(Damage.ApplyDamage(COMPONENT_DOOR_BOOT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
						SetDoorDamage(CAR_BOOT, DOOR_BOOT);
						doubleMoney = true;
					}
				}
				break;

			case CAR_PIECE_DOOR_LF:
				GetComponentWorldPosition(CAR_DOOR_LF, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if((m_nDoorLock == CARLOCK_NOT_USED || m_nDoorLock == CARLOCK_UNLOCKED) &&
				   Damage.ApplyDamage(COMPONENT_DOOR_FRONT_LEFT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetDoorDamage(CAR_DOOR_LF, DOOR_FRONT_LEFT);
					doubleMoney = true;
				}
				break;
			case CAR_PIECE_DOOR_RF:
				GetComponentWorldPosition(CAR_DOOR_RF, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if((m_nDoorLock == CARLOCK_NOT_USED || m_nDoorLock == CARLOCK_UNLOCKED) &&
				   Damage.ApplyDamage(COMPONENT_DOOR_FRONT_RIGHT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetDoorDamage(CAR_DOOR_RF, DOOR_FRONT_RIGHT);
					doubleMoney = true;
				}
				break;
			case CAR_PIECE_DOOR_LR:
				GetComponentWorldPosition(CAR_DOOR_LR, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if((m_nDoorLock == CARLOCK_NOT_USED || m_nDoorLock == CARLOCK_UNLOCKED) &&
				   Damage.ApplyDamage(COMPONENT_DOOR_REAR_LEFT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetDoorDamage(CAR_DOOR_LR, DOOR_REAR_LEFT);
					doubleMoney = true;
				}
				break;
			case CAR_PIECE_DOOR_RR:
				GetComponentWorldPosition(CAR_DOOR_RR, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if((m_nDoorLock == CARLOCK_NOT_USED || m_nDoorLock == CARLOCK_UNLOCKED) &&
				   Damage.ApplyDamage(COMPONENT_DOOR_REAR_RIGHT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetDoorDamage(CAR_DOOR_RR, DOOR_REAR_RIGHT);
					doubleMoney = true;
				}
				break;

			case CAR_PIECE_WING_LF:
				GetComponentWorldPosition(CAR_WING_LF, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if(Damage.ApplyDamage(COMPONENT_PANEL_FRONT_LEFT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetPanelDamage(CAR_WING_LF, VEHPANEL_FRONT_LEFT);
					doubleMoney = true;
				}
				break;
			case CAR_PIECE_WING_RF:
				GetComponentWorldPosition(CAR_WING_RF, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if(Damage.ApplyDamage(COMPONENT_PANEL_FRONT_RIGHT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetPanelDamage(CAR_WING_RF, VEHPANEL_FRONT_RIGHT);
					doubleMoney = true;
				}
				break;
			case CAR_PIECE_WING_LR:
				GetComponentWorldPosition(CAR_WING_LR, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if(Damage.ApplyDamage(COMPONENT_PANEL_REAR_LEFT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetPanelDamage(CAR_WING_LR, VEHPANEL_REAR_LEFT);
					doubleMoney = true;
				}
				break;
			case CAR_PIECE_WING_RR:
				GetComponentWorldPosition(CAR_WING_RR, pos);
				dmgDrawCarCollidingParticles(pos, impulse);
				if(Damage.ApplyDamage(COMPONENT_PANEL_REAR_RIGHT, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					SetPanelDamage(CAR_WING_RR, VEHPANEL_REAR_RIGHT);
					doubleMoney = true;
				}
				break;

			case CAR_PIECE_WHEEL_LF:
			case CAR_PIECE_WHEEL_LR:
			case CAR_PIECE_WHEEL_RF:
			case CAR_PIECE_WHEEL_RR:
				break;

			case CAR_PIECE_WINDSCREEN:
				if(Damage.ApplyDamage(COMPONENT_PANEL_WINDSCREEN, impulse*impulseMult, pHandling->fCollisionDamageMultiplier)){
					uint8 oldStatus = Damage.GetPanelStatus(VEHPANEL_WINDSCREEN);
					SetPanelDamage(CAR_WINDSCREEN, VEHPANEL_WINDSCREEN);
					if(oldStatus != Damage.GetPanelStatus(VEHPANEL_WINDSCREEN)){
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_WINDSHIELD_CRACK, 0.0f);
						doubleMoney = true;
					}
				}
				break;
			}

			if(m_pDamageEntity && m_pDamageEntity == FindPlayerVehicle() && impulse > 10.0f){
				int money = (doubleMoney ? 2 : 1) * impulse*pHandling->nMonetaryValue/1000000.0f;
				money = min(money, 40);
				if(money > 2){
					sprintf(gString, "$%d", money);
					CWorld::Players[CWorld::PlayerInFocus].m_nMoney += money;
				}
			}
		}

		float damage = (impulse-25.0f)*pHandling->fCollisionDamageMultiplier*0.6f*damageMultiplier;

		if(GetModelIndex() == MI_SECURICA && m_pDamageEntity && m_pDamageEntity->m_status == STATUS_PLAYER)
			damage *= 7.0f;

		if(damage > 0.0f){
			int oldHealth = m_fHealth;
			if(this == FindPlayerVehicle()){
				m_fHealth -= bTakeLessDamage ? damage/6.0f : damage/2.0f;
			}else{
				if(damage > 35.0f && pDriver)
					pDriver->Say(SOUND_PED_CAR_COLLISION);
				m_fHealth -= bTakeLessDamage ? damage/12.0f : damage/4.0f;
			}
			if(m_fHealth <= 0.0f && oldHealth > 0)
				m_fHealth = 1.0f;
		}

		// play sound if a light broke
		for(i = 0; i < 4; i++)
			if(oldLightStatus[i] != 1 && Damage.GetLightStatus((eLights)i) == 1){
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_LIGHT_BREAK, i);	// BUG? i?
				break;
			}
	}

	if(m_fHealth < 250.0f){
		// Car is on fire
		if(Damage.GetEngineStatus() < ENGINE_STATUS_ON_FIRE){
			// Set engine on fire and remember who did this
			Damage.SetEngineStatus(ENGINE_STATUS_ON_FIRE);
			m_fFireBlowUpTimer = 0.0f;
			m_pSetOnFireEntity = m_pDamageEntity;
			if(m_pSetOnFireEntity)
				m_pSetOnFireEntity->RegisterReference(&m_pSetOnFireEntity);
		}
	}else{
		if(GetModelIndex() == MI_BFINJECT){
			if(m_fHealth < 400.0f)
				Damage.SetEngineStatus(200);
			else if(m_fHealth < 600.0f)
				Damage.SetEngineStatus(100);
		}
	}
}
#endif

void
CAutomobile::dmgDrawCarCollidingParticles(const CVector &pos, float amount)
{
	int i, n;

	if(!GetIsOnScreen())
		return;

	// FindPlayerSpeed() unused

	n = (int)amount/20;

	for(i = 0; i < ((n+4)&0x1F); i++)
		CParticle::AddParticle(PARTICLE_SPARK_SMALL, pos,
			CVector(CGeneral::GetRandomNumberInRange(-0.1f, 0.1f),
			        CGeneral::GetRandomNumberInRange(-0.1f, 0.1f),
			        0.006f));

	for(i = 0; i < n+2; i++)
		CParticle::AddParticle(PARTICLE_CARCOLLISION_DUST,
			CVector(CGeneral::GetRandomNumberInRange(-1.2f, 1.2f) + pos.x,
			        CGeneral::GetRandomNumberInRange(-1.2f, 1.2f) + pos.y,
			        pos.z),
			CVector(0.0f, 0.0f, 0.0f), nil, 0.5f);

	n = (int)amount/50 + 1;
	for(i = 0; i < n; i++)
		CParticle::AddParticle(PARTICLE_CAR_DEBRIS, pos,
			CVector(CGeneral::GetRandomNumberInRange(-0.25f, 0.25f),
			        CGeneral::GetRandomNumberInRange(-0.25f, 0.25f),
			        CGeneral::GetRandomNumberInRange(0.1f, 0.25f)),
			nil,
			CGeneral::GetRandomNumberInRange(0.02f, 0.08f),
			CVehicleModelInfo::ms_vehicleColourTable[m_currentColour1],
			CGeneral::GetRandomNumberInRange(-40.0f, 40.0f),
			0,
			CGeneral::GetRandomNumberInRange(0.0f, 4.0f));
}

void
CAutomobile::GetComponentWorldPosition(int32 component, CVector &pos)
{
	if(m_aCarNodes[component] == nil){
		printf("CarNode missing: %d %d\n", GetModelIndex(), component);
		return;
	}
	RwMatrix *ltm = RwFrameGetLTM(m_aCarNodes[component]);
	pos = *RwMatrixGetPos(ltm);
}

bool
CAutomobile::IsComponentPresent(int32 comp)
{
	return m_aCarNodes[comp] != nil;
}

void
CAutomobile::SetComponentRotation(int32 component, CVector rotation)
{
	CMatrix mat(RwFrameGetMatrix(m_aCarNodes[component]));
	CVector pos = mat.GetPosition();
	// BUG: all these set the whole matrix
	mat.SetRotateX(DEGTORAD(rotation.x));
	mat.SetRotateY(DEGTORAD(rotation.y));
	mat.SetRotateZ(DEGTORAD(rotation.z));
	mat.Translate(pos);
	mat.UpdateRW();
}

void
CAutomobile::OpenDoor(int32 component, eDoors door, float openRatio)
{
	CMatrix mat(RwFrameGetMatrix(m_aCarNodes[component]));
	CVector pos = mat.GetPosition();
	float axes[3] = { 0.0f, 0.0f, 0.0f };
	float wasClosed = false;

	if(Doors[door].IsClosed()){
		// enable angle cull for closed doors
		RwFrameForAllObjects(m_aCarNodes[component], CVehicleModelInfo::ClearAtomicFlagCB, (void*)ATOMIC_FLAG_NOCULL);
		wasClosed = true;
	}

	Doors[door].Open(openRatio);

	if(wasClosed && Doors[door].RetAngleWhenClosed() != Doors[door].m_fAngle){
		// door opened
		HideAllComps();
		// turn off angle cull for swinging door
		RwFrameForAllObjects(m_aCarNodes[component], CVehicleModelInfo::SetAtomicFlagCB, (void*)ATOMIC_FLAG_NOCULL);
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_DOOR_OPEN_BONNET + door, 0.0f);
	}

	if(!wasClosed && openRatio == 0.0f){
		// door closed
		if(Damage.GetDoorStatus(door) == DOOR_STATUS_SWINGING)
			Damage.SetDoorStatus(door, DOOR_STATUS_OK);	// huh?
		ShowAllComps();
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_DOOR_CLOSE_BONNET + door, 0.0f);
	}

	axes[Doors[door].m_nAxis] = Doors[door].m_fAngle;
	mat.SetRotate(axes[0], axes[1], axes[2]);
	mat.Translate(pos);
	mat.UpdateRW();
}

inline void ProcessDoorOpenAnimation(CAutomobile *car, uint32 component, eDoors door, float time, float start, float end)
{
	if(time > start && time < end){
		float ratio = (time - start)/(end - start);
		if(car->Doors[door].GetAngleOpenRatio() < ratio)
			car->OpenDoor(component, door, ratio);
	}else if(time > end){
		car->OpenDoor(component, door, 1.0f);
	}
}

inline void ProcessDoorCloseAnimation(CAutomobile *car, uint32 component, eDoors door, float time, float start, float end)
{
	if(time > start && time < end){
		float ratio = 1.0f - (time - start)/(end - start);
		if(car->Doors[door].GetAngleOpenRatio() > ratio)
			car->OpenDoor(component, door, ratio);
	}else if(time > end){
		car->OpenDoor(component, door, 0.0f);
	}
}

inline void ProcessDoorOpenCloseAnimation(CAutomobile *car, uint32 component, eDoors door, float time, float start, float mid, float end)
{
	if(time > start && time < mid){
		// open
		float ratio = (time - start)/(mid - start);
		if(car->Doors[door].GetAngleOpenRatio() < ratio)
			car->OpenDoor(component, door, ratio);
	}else if(time > mid && time < end){
		// close
		float ratio = 1.0f - (time - mid)/(end - mid);
		if(car->Doors[door].GetAngleOpenRatio() > ratio)
			car->OpenDoor(component, door, ratio);
	}else if(time > end){
		car->OpenDoor(component, door, 0.0f);
	}
}
void
CAutomobile::ProcessOpenDoor(uint32 component, uint32 anim, float time)
{
	eDoors door;

	switch(component){
	case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
	case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
	case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
	case CAR_DOOR_LR: door = DOOR_REAR_LEFT; break;
	default: assert(0);
	}

	if(IsDoorMissing(door))
		return;

	switch(anim){
	case ANIM_CAR_QJACK:
	case ANIM_CAR_OPEN_LHS:
	case ANIM_CAR_OPEN_RHS:
		ProcessDoorOpenAnimation(this, component, door, time, 0.66f, 0.8f);
		break;
	case ANIM_CAR_CLOSEDOOR_LHS:
	case ANIM_CAR_CLOSEDOOR_LOW_LHS:
	case ANIM_CAR_CLOSEDOOR_RHS:
	case ANIM_CAR_CLOSEDOOR_LOW_RHS:
		ProcessDoorCloseAnimation(this, component, door, time, 0.2f, 0.63f);
		break;
	case ANIM_CAR_ROLLDOOR:
	case ANIM_CAR_ROLLDOOR_LOW:
		ProcessDoorOpenCloseAnimation(this, component, door, time, 0.1f, 0.6f, 0.95f);
		break;
		break;
	case ANIM_CAR_GETOUT_LHS:
	case ANIM_CAR_GETOUT_LOW_LHS:
	case ANIM_CAR_GETOUT_RHS:
	case ANIM_CAR_GETOUT_LOW_RHS:
		ProcessDoorOpenAnimation(this, component, door, time, 0.06f, 0.43f);
		break;
	case ANIM_CAR_CLOSE_LHS:
	case ANIM_CAR_CLOSE_RHS:
		ProcessDoorCloseAnimation(this, component, door, time, 0.1f, 0.23f);
		break;
	case ANIM_CAR_PULLOUT_RHS:
	case ANIM_CAR_PULLOUT_LOW_RHS:
		OpenDoor(component, door, 1.0f);
	case ANIM_COACH_OPEN_L:
	case ANIM_COACH_OPEN_R:
		ProcessDoorOpenAnimation(this, component, door, time, 0.66f, 0.8f);
		break;
	case ANIM_COACH_OUT_L:
		ProcessDoorOpenAnimation(this, component, door, time, 0.0f, 0.3f);
		break;
	case ANIM_VAN_OPEN_L:
	case ANIM_VAN_OPEN:
		ProcessDoorOpenAnimation(this, component, door, time, 0.37f, 0.55f);
		break;
	case ANIM_VAN_CLOSE_L:
	case ANIM_VAN_CLOSE:
		ProcessDoorCloseAnimation(this, component, door, time, 0.5f, 0.8f);
		break;
	case ANIM_VAN_GETOUT_L:
	case ANIM_VAN_GETOUT:
		ProcessDoorOpenAnimation(this, component, door, time, 0.5f, 0.6f);
		break;
	case NUM_ANIMS:
		OpenDoor(component, door, time);
		break;
	}
}

bool
CAutomobile::IsDoorReady(eDoors door)
{
	if(Doors[door].IsClosed() || IsDoorMissing(door))
		return true;
	int doorflag = 0;
	// TODO: enum?
	switch(door){
	case DOOR_FRONT_LEFT: doorflag = 1; break;
	case DOOR_FRONT_RIGHT: doorflag = 4; break;
	case DOOR_REAR_LEFT: doorflag = 2; break;
	case DOOR_REAR_RIGHT: doorflag = 8; break;
	}
	return (doorflag & m_nGettingInFlags) == 0;
}

bool
CAutomobile::IsDoorFullyOpen(eDoors door)
{
	return Doors[door].IsFullyOpen() || IsDoorMissing(door);
}

bool
CAutomobile::IsDoorClosed(eDoors door)
{
	return !!Doors[door].IsClosed();
}

bool
CAutomobile::IsDoorMissing(eDoors door)
{
	return Damage.GetDoorStatus(door) == DOOR_STATUS_MISSING;
}

void
CAutomobile::RemoveRefsToVehicle(CEntity *ent)
{
	int i;
	for(i = 0; i < 4; i++)
		if(m_aGroundPhysical[i] == ent)
			m_aGroundPhysical[i] = nil;
}

void
CAutomobile::BlowUpCar(CEntity *culprit)
{
	int i;
	RpAtomic *atomic;

	if(!bCanBeDamaged)
		return;

	// explosion pushes vehicle up
	m_vecMoveSpeed.z += 0.13f;
	m_status = STATUS_WRECKED;
	bRenderScorched = true;
	m_nTimeOfDeath = CTimer::GetTimeInMilliseconds();
	Damage.FuckCarCompletely();

	if(GetModelIndex() != MI_RCBANDIT){
		SetBumperDamage(CAR_BUMP_FRONT, VEHBUMPER_FRONT);
		SetBumperDamage(CAR_BUMP_REAR, VEHBUMPER_REAR);
		SetDoorDamage(CAR_BONNET, DOOR_BONNET);
		SetDoorDamage(CAR_BOOT, DOOR_BOOT);
		SetDoorDamage(CAR_DOOR_LF, DOOR_FRONT_LEFT);
		SetDoorDamage(CAR_DOOR_RF, DOOR_FRONT_RIGHT);
		SetDoorDamage(CAR_DOOR_LR, DOOR_REAR_LEFT);
		SetDoorDamage(CAR_DOOR_RR, DOOR_REAR_RIGHT);
		SpawnFlyingComponent(CAR_WHEEL_LF, COMPGROUP_WHEEL);
		atomic = nil;
		RwFrameForAllObjects(m_aCarNodes[CAR_WHEEL_LF], GetCurrentAtomicObjectCB, &atomic);
		if(atomic)
			RpAtomicSetFlags(atomic, 0);
	}

	m_fHealth = 0.0f;
	m_nBombTimer = 0;
	m_bombType = CARBOMB_NONE;

	TheCamera.CamShake(0.7f, GetPosition().x, GetPosition().y, GetPosition().z);

	// kill driver and passengers
	if(pDriver){
		CDarkel::RegisterKillByPlayer(pDriver, WEAPONTYPE_EXPLOSION);
		if(pDriver->GetPedState() == PED_DRIVING){
			pDriver->SetDead();
			if(!pDriver->IsPlayer())
				pDriver->FlagToDestroyWhenNextProcessed();
		}else
			pDriver->SetDie(ANIM_KO_SHOT_FRONT1, 4.0f, 0.0f);
	}
	for(i = 0; i < m_nNumMaxPassengers; i++){
		if(pPassengers[i]){
			CDarkel::RegisterKillByPlayer(pPassengers[i], WEAPONTYPE_EXPLOSION);
			if(pPassengers[i]->GetPedState() == PED_DRIVING){
				pPassengers[i]->SetDead();
				if(!pPassengers[i]->IsPlayer())
					pPassengers[i]->FlagToDestroyWhenNextProcessed();
			}else
				pPassengers[i]->SetDie(ANIM_KO_SHOT_FRONT1, 4.0f, 0.0f);
		}
	}

	bEngineOn = false;
	bLightsOn = false;
	m_bSirenOrAlarm = false;
	bTaxiLight = false;
	if(bIsAmbulanceOnDuty){
		bIsAmbulanceOnDuty = false;
		CCarCtrl::NumAmbulancesOnDuty--;
	}
	if(bIsFireTruckOnDuty){
		bIsFireTruckOnDuty = false;
		CCarCtrl::NumFiretrucksOnDuty--;
	}
	ChangeLawEnforcerState(false);

	gFireManager.StartFire(this, culprit, 0.8f, 1);	// TODO
	CDarkel::RegisterCarBlownUpByPlayer(this);
	if(GetModelIndex() == MI_RCBANDIT)
		CExplosion::AddExplosion(this, culprit, EXPLOSION_4, GetPosition(), 0);	// TODO
	else
		CExplosion::AddExplosion(this, culprit, EXPLOSION_3, GetPosition(), 0);	// TODO
}

bool
CAutomobile::SetUpWheelColModel(CColModel *colModel)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CColModel *vehColModel = mi->GetColModel();

	colModel->boundingSphere = vehColModel->boundingSphere;
	colModel->boundingBox = vehColModel->boundingBox;

	CMatrix mat;
	mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LF]));
	colModel->spheres[0].Set(mi->m_wheelScale, mat.GetPosition(), SURFACE_TIRE, CAR_PIECE_WHEEL_LF);
	mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LB]));
	colModel->spheres[1].Set(mi->m_wheelScale, mat.GetPosition(), SURFACE_TIRE, CAR_PIECE_WHEEL_LR);
	mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RF]));
	colModel->spheres[2].Set(mi->m_wheelScale, mat.GetPosition(), SURFACE_TIRE, CAR_PIECE_WHEEL_RF);
	mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RB]));
	colModel->spheres[3].Set(mi->m_wheelScale, mat.GetPosition(), SURFACE_TIRE, CAR_PIECE_WHEEL_RR);

	if(m_aCarNodes[CAR_WHEEL_LM] != nil && m_aCarNodes[CAR_WHEEL_RM] != nil){
		mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LM]));
		colModel->spheres[4].Set(mi->m_wheelScale, mat.GetPosition(), SURFACE_TIRE, CAR_PIECE_WHEEL_RF);
		mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RM]));
		colModel->spheres[5].Set(mi->m_wheelScale, mat.GetPosition(), SURFACE_TIRE, CAR_PIECE_WHEEL_RR);
		colModel->numSpheres = 6;
	}else
		colModel->numSpheres = 4;

	return true;
}

// this probably isn't used in III yet
void
CAutomobile::BurstTyre(uint8 wheel)
{
	switch(wheel){
	case CAR_PIECE_WHEEL_LF: wheel = VEHWHEEL_FRONT_LEFT; break;
	case CAR_PIECE_WHEEL_LR: wheel = VEHWHEEL_REAR_LEFT; break;
	case CAR_PIECE_WHEEL_RF: wheel = VEHWHEEL_FRONT_RIGHT; break;
	case CAR_PIECE_WHEEL_RR: wheel = VEHWHEEL_REAR_RIGHT; break;
	}

	int status = Damage.GetWheelStatus(wheel);
	if(status == WHEEL_STATUS_OK){
		Damage.SetWheelStatus(wheel, WHEEL_STATUS_BURST);

		if(m_status == STATUS_SIMPLE){
			m_status = STATUS_PHYSICS;
			CCarCtrl::SwitchVehicleToRealPhysics(this);
		}

		ApplyMoveForce(GetRight() * CGeneral::GetRandomNumberInRange(-0.3f, 0.3f));
		ApplyTurnForce(GetRight() * CGeneral::GetRandomNumberInRange(-0.3f, 0.3f), GetForward());
	}
}

WRAPPER bool CAutomobile::IsRoomForPedToLeaveCar(uint32, CVector *) { EAXJMP(0x53C5B0); }

float
CAutomobile::GetHeightAboveRoad(void)
{
	return m_fHeightAboveRoad;
}

void
CAutomobile::PlayCarHorn(void)
{
	int r;

	if(m_nCarHornTimer != 0)
		return;

	r = CGeneral::GetRandomNumber() & 7;
	if(r < 2){
		m_nCarHornTimer = 45;
	}else if(r < 4){
		if(pDriver)
			pDriver->Say(SOUND_PED_CAR_COLLISION);
		m_nCarHornTimer = 45;
	}else{
		if(pDriver)
			pDriver->Say(SOUND_PED_CAR_COLLISION);
	}
}

void
CAutomobile::PlayHornIfNecessary(void)
{
	if(AutoPilot.m_flag2 ||
	   AutoPilot.m_flag1)
		if(!HasCarStoppedBecauseOfLight())
			PlayCarHorn();
}


void
CAutomobile::ResetSuspension(void)
{
	int i;
	for(i = 0; i < 4; i++){
		m_aSuspensionSpringRatio[i] = 1.0f;
		m_aWheelTimer[i] = 0.0f;
		m_aWheelRotation[i] = 0.0f;
		m_aWheelState[i] = WHEEL_STATE_0;
	}
}

void
CAutomobile::SetupSuspensionLines(void)
{
	int i;
	CVector posn;
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
	CColModel *colModel = mi->GetColModel();

	// Each suspension line starts at the uppermost wheel position
	// and extends down to the lowermost point on the tyre
	for(i = 0; i < 4; i++){
		mi->GetWheelPosn(i, posn);
		m_aWheelPosition[i] = posn.z;

		// uppermost wheel position
		posn.z += pHandling->fSuspensionUpperLimit;
		colModel->lines[i].p0 = posn;

		// lowermost wheel position
		posn.z += pHandling->fSuspensionLowerLimit - pHandling->fSuspensionUpperLimit;
		// lowest point on tyre
		posn.z -= mi->m_wheelScale*0.5f;
		colModel->lines[i].p1 = posn;

		// this is length of the spring at rest
		m_aSuspensionSpringLength[i] = pHandling->fSuspensionUpperLimit - pHandling->fSuspensionLowerLimit;
		m_aSuspensionLineLength[i] = colModel->lines[i].p0.z - colModel->lines[i].p1.z;
	}

	// Compress spring somewhat to get normal height on road
	m_fHeightAboveRoad = -(colModel->lines[0].p0.z + (colModel->lines[0].p1.z - colModel->lines[0].p0.z)*
	                                                  (1.0f - 1.0f/(8.0f*pHandling->fSuspensionForceLevel)));
	for(i = 0; i < 4; i++)
		m_aWheelPosition[i] = mi->m_wheelScale*0.5f - m_fHeightAboveRoad;

	// adjust col model to include suspension lines
	if(colModel->boundingBox.min.z > colModel->lines[0].p1.z)
		colModel->boundingBox.min.z = colModel->lines[0].p1.z;
	float radius = max(colModel->boundingBox.min.Magnitude(), colModel->boundingBox.max.Magnitude());
	if(colModel->boundingSphere.radius < radius)
		colModel->boundingSphere.radius = radius;

	if(GetModelIndex() == MI_RCBANDIT){
		colModel->boundingSphere.radius = 2.0f;
		for(i = 0; i < colModel->numSpheres; i++)
			colModel->spheres[i].radius = 0.3f;
	}
}

// called on police cars
void
CAutomobile::ScanForCrimes(void)
{
	if(FindPlayerVehicle() && FindPlayerVehicle()->IsCar())
		if(FindPlayerVehicle()->IsAlarmOn())
			// if player's alarm is on, increase wanted level
			if((FindPlayerVehicle()->GetPosition() - GetPosition()).MagnitudeSqr() < sq(20.0f))
				CWorld::Players[CWorld::PlayerInFocus].m_pPed->SetWantedLevelNoDrop(1);
}

void
CAutomobile::BlowUpCarsInPath(void)
{
	int i;

	if(m_vecMoveSpeed.Magnitude() > 0.1f)
		for(i = 0; i < m_nCollisionRecords; i++)
			if(m_aCollisionRecords[i] &&
			   m_aCollisionRecords[i]->IsVehicle() &&
			   m_aCollisionRecords[i]->GetModelIndex() != MI_RHINO &&
			   !m_aCollisionRecords[i]->bRenderScorched)
				((CVehicle*)m_aCollisionRecords[i])->BlowUpCar(this);
}

bool
CAutomobile::HasCarStoppedBecauseOfLight(void)
{
	int i;

	if(m_status != STATUS_SIMPLE && m_status != STATUS_PHYSICS)
		return false;

	if(AutoPilot.m_nCurrentRouteNode && AutoPilot.m_nNextRouteNode){
		CPathNode *curnode = &ThePaths.m_pathNodes[AutoPilot.m_nCurrentRouteNode];
		for(i = 0; i < curnode->numLinks; i++)
			if(ThePaths.m_connections[curnode->firstLink + i] == AutoPilot.m_nNextRouteNode)
				break;
		if(i < curnode->numLinks &&
		   ThePaths.m_carPathLinks[ThePaths.m_carPathConnections[curnode->firstLink + i]].trafficLightType & 3)	// TODO
			return true;
	}

	if(AutoPilot.m_nCurrentRouteNode && AutoPilot.m_nPrevRouteNode){
		CPathNode *curnode = &ThePaths.m_pathNodes[AutoPilot.m_nCurrentRouteNode];
		for(i = 0; i < curnode->numLinks; i++)
			if(ThePaths.m_connections[curnode->firstLink + i] == AutoPilot.m_nPrevRouteNode)
				break;
		if(i < curnode->numLinks &&
		   ThePaths.m_carPathLinks[ThePaths.m_carPathConnections[curnode->firstLink + i]].trafficLightType & 3)	// TODO
			return true;
	}

	return false;
}

void
CAutomobile::SetBusDoorTimer(uint32 timer, uint8 type)
{
	if(timer < 1000)
		timer = 1000;
	if(type == 0)
		// open and close
		m_nBusDoorTimerStart = CTimer::GetTimeInMilliseconds();
	else
		// only close
		m_nBusDoorTimerStart = CTimer::GetTimeInMilliseconds() - 500;
	m_nBusDoorTimerEnd = m_nBusDoorTimerStart + timer;
}

void
CAutomobile::ProcessAutoBusDoors(void)
{
	if(CTimer::GetTimeInMilliseconds() < m_nBusDoorTimerEnd){
		if(m_nBusDoorTimerEnd != 0 && CTimer::GetTimeInMilliseconds() > m_nBusDoorTimerEnd-500){
			// close door
			if(!IsDoorMissing(DOOR_FRONT_LEFT) && (m_nGettingInFlags & 1) == 0){
				if(IsDoorClosed(DOOR_FRONT_LEFT)){
					m_nBusDoorTimerEnd = CTimer::GetTimeInMilliseconds();
					OpenDoor(CAR_DOOR_LF, DOOR_FRONT_LEFT, 0.0f);
				}else{
					OpenDoor(CAR_DOOR_LF, DOOR_FRONT_LEFT,
						1.0f - (CTimer::GetTimeInMilliseconds() - (m_nBusDoorTimerEnd-500))/500.0f);
				}
			}

			if(!IsDoorMissing(DOOR_FRONT_RIGHT) && (m_nGettingInFlags & 4) == 0){
				if(IsDoorClosed(DOOR_FRONT_RIGHT)){
					m_nBusDoorTimerEnd = CTimer::GetTimeInMilliseconds();
					OpenDoor(CAR_DOOR_RF, DOOR_FRONT_RIGHT, 0.0f);
				}else{
					OpenDoor(CAR_DOOR_RF, DOOR_FRONT_RIGHT,
						1.0f - (CTimer::GetTimeInMilliseconds() - (m_nBusDoorTimerEnd-500))/500.0f);
				}
			}
		}
	}else{
		// ended
		if(m_nBusDoorTimerStart){
			if(!IsDoorMissing(DOOR_FRONT_LEFT) && (m_nGettingInFlags & 1) == 0)
				OpenDoor(CAR_DOOR_LF, DOOR_FRONT_LEFT, 0.0f);
			if(!IsDoorMissing(DOOR_FRONT_RIGHT) && (m_nGettingInFlags & 4) == 0)
				OpenDoor(CAR_DOOR_RF, DOOR_FRONT_RIGHT, 0.0f);
			m_nBusDoorTimerStart = 0;
			m_nBusDoorTimerEnd = 0;
		}
	}
}

void
CAutomobile::ProcessSwingingDoor(int32 component, eDoors door)
{
	if(Damage.GetDoorStatus(door) != DOOR_STATUS_SWINGING)
		return;

	CMatrix mat(RwFrameGetMatrix(m_aCarNodes[component]));
	CVector pos = mat.GetPosition();
	float axes[3] = { 0.0f, 0.0f, 0.0f };

	Doors[door].Process(this);
	axes[Doors[door].m_nAxis] = Doors[door].m_fAngle;
	mat.SetRotate(axes[0], axes[1], axes[2]);
	mat.Translate(pos);
	mat.UpdateRW();
}

void
CAutomobile::Fix(void)
{
	int component;

	Damage.ResetDamageStatus();

	if(pHandling->Flags & HANDLING_NO_DOORS){
		Damage.SetDoorStatus(DOOR_FRONT_LEFT, DOOR_STATUS_MISSING);
		Damage.SetDoorStatus(DOOR_FRONT_RIGHT, DOOR_STATUS_MISSING);
		Damage.SetDoorStatus(DOOR_REAR_LEFT, DOOR_STATUS_MISSING);
		Damage.SetDoorStatus(DOOR_REAR_RIGHT, DOOR_STATUS_MISSING);
	}

	bIsDamaged = false;
	RpClumpForAllAtomics((RpClump*)m_rwObject, CVehicleModelInfo::HideAllComponentsAtomicCB, (void*)ATOMIC_FLAG_DAM);

	for(component = CAR_BUMP_FRONT; component < NUM_CAR_NODES; component++){
		if(m_aCarNodes[component]){
			CMatrix mat(RwFrameGetMatrix(m_aCarNodes[component]));
			mat.SetTranslate(mat.GetPosition());
			mat.UpdateRW();
		}
	}
}

void
CAutomobile::SetupDamageAfterLoad(void)
{
	if(m_aCarNodes[CAR_BUMP_FRONT])
		SetBumperDamage(CAR_BUMP_FRONT, VEHBUMPER_FRONT);
	if(m_aCarNodes[CAR_BONNET])
		SetDoorDamage(CAR_BONNET, DOOR_BONNET);
	if(m_aCarNodes[CAR_BUMP_REAR])
		SetBumperDamage(CAR_BUMP_REAR, VEHBUMPER_REAR);
	if(m_aCarNodes[CAR_BOOT])
		SetDoorDamage(CAR_BOOT, DOOR_BOOT);
	if(m_aCarNodes[CAR_DOOR_LF])
		SetDoorDamage(CAR_DOOR_LF, DOOR_FRONT_LEFT);
	if(m_aCarNodes[CAR_DOOR_RF])
		SetDoorDamage(CAR_DOOR_RF, DOOR_FRONT_RIGHT);
	if(m_aCarNodes[CAR_DOOR_LR])
		SetDoorDamage(CAR_DOOR_LR, DOOR_REAR_LEFT);
	if(m_aCarNodes[CAR_DOOR_RR])
		SetDoorDamage(CAR_DOOR_RR, DOOR_REAR_RIGHT);
	if(m_aCarNodes[CAR_WING_LF])
		SetPanelDamage(CAR_WING_LF, VEHPANEL_FRONT_LEFT);
	if(m_aCarNodes[CAR_WING_RF])
		SetPanelDamage(CAR_WING_RF, VEHPANEL_FRONT_RIGHT);
	if(m_aCarNodes[CAR_WING_LR])
		SetPanelDamage(CAR_WING_LR, VEHPANEL_REAR_LEFT);
	if(m_aCarNodes[CAR_WING_RR])
		SetPanelDamage(CAR_WING_RR, VEHPANEL_REAR_RIGHT);
}

RwObject*
GetCurrentAtomicObjectCB(RwObject *object, void *data)
{
	RpAtomic *atomic = (RpAtomic*)object;
	assert(RwObjectGetType(object) == rpATOMIC);
	if(RpAtomicGetFlags(atomic) & rpATOMICRENDER)
		*(RpAtomic**)data = atomic;
	return object;
}

CColPoint aTempPedColPts[32];	// this name doesn't make any sense

CObject*
CAutomobile::SpawnFlyingComponent(int32 component, uint32 type)
{
	RpAtomic *atomic;
	RwFrame *frame;
	RwMatrix *matrix;
	CObject *obj;

	if(CObject::nNoTempObjects >= NUMTEMPOBJECTS)
		return nil;

	atomic = nil;
	RwFrameForAllObjects(m_aCarNodes[component], GetCurrentAtomicObjectCB, &atomic);
	if(atomic == nil)
		return nil;

	obj = new CObject;
	if(obj == nil)
		return nil;

	if(component == CAR_WINDSCREEN){
		obj->SetModelIndexNoCreate(MI_CAR_BONNET);
	}else switch(type){
	case COMPGROUP_BUMPER:
		obj->SetModelIndexNoCreate(MI_CAR_BUMPER);
		break;
	case COMPGROUP_WHEEL:
		obj->SetModelIndexNoCreate(MI_CAR_WHEEL);
		break;
	case COMPGROUP_DOOR:
		obj->SetModelIndexNoCreate(MI_CAR_DOOR);
		obj->SetCenterOfMass(0.0f, -0.5f, 0.0f);
		break;
	case COMPGROUP_BONNET:
		obj->SetModelIndexNoCreate(MI_CAR_BONNET);
		obj->SetCenterOfMass(0.0f, 0.4f, 0.0f);
		break;
	case COMPGROUP_BOOT:
		obj->SetModelIndexNoCreate(MI_CAR_BOOT);
		obj->SetCenterOfMass(0.0f, -0.3f, 0.0f);
		break;
	case COMPGROUP_PANEL:
	default:
		obj->SetModelIndexNoCreate(MI_CAR_PANEL);
		break;
	}

	// object needs base model
	obj->RefModelInfo(GetModelIndex());

	// create new atomic
	matrix = RwFrameGetLTM(m_aCarNodes[component]);
	frame = RwFrameCreate();
	atomic = RpAtomicClone(atomic);
	*RwFrameGetMatrix(frame) = *matrix;
	RpAtomicSetFrame(atomic, frame);
	CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	obj->AttachToRwObject((RwObject*)atomic);

	// init object
	obj->m_fMass = 10.0f;
	obj->m_fTurnMass = 25.0f;
	obj->m_fAirResistance = 0.97f;
	obj->m_fElasticity = 0.1f;
	obj->m_fBuoyancy = obj->m_fMass*GRAVITY/0.75f;
	obj->ObjectCreatedBy = TEMP_OBJECT;
	obj->bIsStatic = false;
	obj->bIsPickup = false;
	obj->bUseVehicleColours = true;
	obj->m_colour1 = m_currentColour1;
	obj->m_colour2 = m_currentColour2;

	// life time - the more objects the are, the shorter this one will live
	CObject::nNoTempObjects++;
	if(CObject::nNoTempObjects > 20)
		obj->m_nEndOfLifeTime = CTimer::GetTimeInMilliseconds() + 20000/5.0f;
	else if(CObject::nNoTempObjects > 10)
		obj->m_nEndOfLifeTime = CTimer::GetTimeInMilliseconds() + 20000/2.0f;
	else
		obj->m_nEndOfLifeTime = CTimer::GetTimeInMilliseconds() + 20000;

	obj->m_vecMoveSpeed = m_vecMoveSpeed;
	if(obj->m_vecMoveSpeed.z > 0.0f){
		obj->m_vecMoveSpeed.z *= 1.5f;
	}else if(GetUp().z > 0.0f &&
	         (component == COMPGROUP_BONNET || component == COMPGROUP_BOOT || component == CAR_WINDSCREEN)){
		obj->m_vecMoveSpeed.z *= -1.5f;
		obj->m_vecMoveSpeed.z += 0.04f;
	}else{
		obj->m_vecMoveSpeed.z *= 0.25f;
	}
	obj->m_vecMoveSpeed.x *= 0.75f;
	obj->m_vecMoveSpeed.y *= 0.75f;

	obj->m_vecTurnSpeed = m_vecTurnSpeed*2.0f;

	// push component away from car
	CVector dist = obj->GetPosition() - GetPosition();
	dist.Normalise();
	if(component == COMPGROUP_BONNET || component == COMPGROUP_BOOT || component == CAR_WINDSCREEN){
		// push these up some
		dist += GetUp();
		if(GetUp().z > 0.0f){
			// simulate fast upward movement if going fast
			float speed = CVector2D(m_vecMoveSpeed).MagnitudeSqr();
			obj->GetPosition() += GetUp()*speed;
		}
	}
	obj->ApplyMoveForce(dist);

	if(type == COMPGROUP_WHEEL){
		obj->m_fTurnMass = 5.0f;
		obj->m_vecTurnSpeed.x = 0.5f;
		obj->m_fAirResistance = 0.99f;
	}

	if(CCollision::ProcessColModels(obj->GetMatrix(), *obj->GetColModel(),
			this->GetMatrix(), *this->GetColModel(),
			aTempPedColPts, nil, nil) > 0)
		obj->m_pCollidingEntity = this;

	if(bRenderScorched)
		obj->bRenderScorched = true;

	CWorld::Add(obj);

	return obj;
}

CObject*
CAutomobile::RemoveBonnetInPedCollision(void)
{
	CObject *obj;

	if(Damage.GetDoorStatus(DOOR_BONNET) != DOOR_STATUS_SWINGING &&
	   Doors[DOOR_BONNET].RetAngleWhenOpen()*0.4f < Doors[DOOR_BONNET].m_fAngle){
		// BUG? why not COMPGROUP_BONNET?
		obj = SpawnFlyingComponent(CAR_BONNET, COMPGROUP_DOOR);
		// make both doors invisible on car
		SetComponentVisibility(m_aCarNodes[CAR_BONNET], ATOMIC_FLAG_NONE);
		Damage.SetDoorStatus(DOOR_BONNET, DOOR_STATUS_MISSING);
		return obj;
	}
	return nil;
}

void
CAutomobile::SetPanelDamage(int32 component, ePanels panel, bool noFlyingComponents)
{
	int status = Damage.GetPanelStatus(panel);
	if(m_aCarNodes[component] == nil)
		return;
	if(status == PANEL_STATUS_SMASHED1){
		// show damaged part
		SetComponentVisibility(m_aCarNodes[component], ATOMIC_FLAG_DAM);
	}else if(status == PANEL_STATUS_MISSING){
		if(!noFlyingComponents)
			SpawnFlyingComponent(component, COMPGROUP_PANEL);
		// hide both
		SetComponentVisibility(m_aCarNodes[component], ATOMIC_FLAG_NONE);
	}
}

void
CAutomobile::SetBumperDamage(int32 component, ePanels panel, bool noFlyingComponents)
{
	int status = Damage.GetPanelStatus(panel);
	if(m_aCarNodes[component] == nil){
		printf("Trying to damage component %d of %s\n",
			component, CModelInfo::GetModelInfo(GetModelIndex())->GetName());
		return;
	}
	if(status == PANEL_STATUS_SMASHED1){
		// show damaged part
		SetComponentVisibility(m_aCarNodes[component], ATOMIC_FLAG_DAM);
	}else if(status == PANEL_STATUS_MISSING){
		if(!noFlyingComponents)
			SpawnFlyingComponent(component, COMPGROUP_BUMPER);
		// hide both
		SetComponentVisibility(m_aCarNodes[component], ATOMIC_FLAG_NONE);
	}
}

void
CAutomobile::SetDoorDamage(int32 component, eDoors door, bool noFlyingComponents)
{
	int status = Damage.GetDoorStatus(door);
	if(m_aCarNodes[component] == nil){
		printf("Trying to damage component %d of %s\n",
			component, CModelInfo::GetModelInfo(GetModelIndex())->GetName());
		return;
	}

	if(door == DOOR_BOOT && status == DOOR_STATUS_SWINGING && pHandling->Flags & HANDLING_NOSWING_BOOT){
		Damage.SetDoorStatus(DOOR_BOOT, DOOR_STATUS_MISSING);
		status = DOOR_STATUS_MISSING;
	}

	if(status == DOOR_STATUS_SMASHED){
		// show damaged part
		SetComponentVisibility(m_aCarNodes[component], ATOMIC_FLAG_DAM);
	}else if(status == DOOR_STATUS_SWINGING){
		// turn off angle cull for swinging doors
		RwFrameForAllObjects(m_aCarNodes[component], CVehicleModelInfo::SetAtomicFlagCB, (void*)ATOMIC_FLAG_NOCULL);
	}else if(status == DOOR_STATUS_MISSING){
		if(!noFlyingComponents){
			if(door == DOOR_BONNET)
				SpawnFlyingComponent(component, COMPGROUP_BONNET);
			else if(door == DOOR_BOOT)
				SpawnFlyingComponent(component, COMPGROUP_BOOT);
			else
				SpawnFlyingComponent(component, COMPGROUP_DOOR);
		}
		// hide both
		SetComponentVisibility(m_aCarNodes[component], ATOMIC_FLAG_NONE);
	}
}


static RwObject*
SetVehicleAtomicVisibilityCB(RwObject *object, void *data)
{
	uint32 flags = (uint32)(uintptr)data;
	RpAtomic *atomic = (RpAtomic*)object;
	if((CVisibilityPlugins::GetAtomicId(atomic) & (ATOMIC_FLAG_OK|ATOMIC_FLAG_DAM)) == flags)
		RpAtomicSetFlags(atomic, rpATOMICRENDER);
	else
		RpAtomicSetFlags(atomic, 0);
	return object;
}

void
CAutomobile::SetComponentVisibility(RwFrame *frame, uint32 flags)
{
	HideAllComps();
	bIsDamaged = true;
	RwFrameForAllObjects(frame, SetVehicleAtomicVisibilityCB, (void*)flags);
}

void
CAutomobile::SetupModelNodes(void)
{
	int i;
	for(i = 0; i < NUM_CAR_NODES; i++)
		m_aCarNodes[i] = nil;
	CClumpModelInfo::FillFrameArray((RpClump*)m_rwObject, m_aCarNodes);
}

void
CAutomobile::SetTaxiLight(bool light)
{
	bTaxiLight = light;
}

bool
CAutomobile::GetAllWheelsOffGround(void)
{
	return m_nDriveWheelsOnGround == 0;
}

void
CAutomobile::HideAllComps(void)
{
	// empty
}

void
CAutomobile::ShowAllComps(void)
{
	// empty
}

void
CAutomobile::ReduceHornCounter(void)
{
	if(m_nCarHornTimer != 0)
		m_nCarHornTimer--;
}

void
CAutomobile::SetAllTaxiLights(bool set)
{
	m_sAllTaxiLights = set;
}

class CAutomobile_ : public CAutomobile
{
public:
	void dtor() { CAutomobile::~CAutomobile(); }
	void SetModelIndex_(uint32 id) { CAutomobile::SetModelIndex(id); }
	void ProcessControl_(void) { CAutomobile::ProcessControl(); }
	void Teleport_(CVector v) { CAutomobile::Teleport(v); }
	void PreRender_(void) { CAutomobile::PreRender(); }
	void Render_(void) { CAutomobile::Render(); }

	int32 ProcessEntityCollision_(CEntity *ent, CColPoint *colpoints){ return CAutomobile::ProcessEntityCollision(ent, colpoints); }

	void ProcessControlInputs_(uint8 pad) { CAutomobile::ProcessControlInputs(pad); }
	void GetComponentWorldPosition_(int32 component, CVector &pos) { CAutomobile::GetComponentWorldPosition(component, pos); }
	bool IsComponentPresent_(int32 component) { return CAutomobile::IsComponentPresent(component); }
	void SetComponentRotation_(int32 component, CVector rotation) { CAutomobile::SetComponentRotation(component, rotation); }
	void OpenDoor_(int32 component, eDoors door, float ratio) { CAutomobile::OpenDoor(component, door, ratio); }
	void ProcessOpenDoor_(uint32 component, uint32 anim, float time) { CAutomobile::ProcessOpenDoor(component, anim, time); }
	bool IsDoorReady_(eDoors door) { return CAutomobile::IsDoorReady(door); }
	bool IsDoorFullyOpen_(eDoors door) { return CAutomobile::IsDoorFullyOpen(door); }
	bool IsDoorClosed_(eDoors door) { return CAutomobile::IsDoorClosed(door); }
	bool IsDoorMissing_(eDoors door) { return CAutomobile::IsDoorMissing(door); }
	void RemoveRefsToVehicle_(CEntity *ent) { CAutomobile::RemoveRefsToVehicle(ent); }
	void BlowUpCar_(CEntity *ent) { CAutomobile::BlowUpCar(ent); }
	bool SetUpWheelColModel_(CColModel *colModel) { return CAutomobile::SetUpWheelColModel(colModel); }
	void BurstTyre_(uint8 tyre) { CAutomobile::BurstTyre(tyre); }
	bool IsRoomForPedToLeaveCar_(uint32 door, CVector *pos) { return CAutomobile::IsRoomForPedToLeaveCar(door, pos); }
	float GetHeightAboveRoad_(void) { return CAutomobile::GetHeightAboveRoad(); }
	void PlayCarHorn_(void) { CAutomobile::PlayCarHorn(); }
};

STARTPATCHES
	InjectHook(0x52D170, &CAutomobile_::dtor, PATCH_JUMP);
	InjectHook(0x52D190, &CAutomobile_::SetModelIndex_, PATCH_JUMP);
	InjectHook(0x531470, &CAutomobile_::ProcessControl_, PATCH_JUMP);
	InjectHook(0x535180, &CAutomobile_::Teleport_, PATCH_JUMP);
	InjectHook(0x53B270, &CAutomobile_::ProcessEntityCollision_, PATCH_JUMP);
	InjectHook(0x53B660, &CAutomobile_::ProcessControlInputs_, PATCH_JUMP);
	InjectHook(0x52E5F0, &CAutomobile_::GetComponentWorldPosition_, PATCH_JUMP);
	InjectHook(0x52E660, &CAutomobile_::IsComponentPresent_, PATCH_JUMP);
	InjectHook(0x52E680, &CAutomobile_::SetComponentRotation_, PATCH_JUMP);
	InjectHook(0x52E750, &CAutomobile_::OpenDoor_, PATCH_JUMP);
	InjectHook(0x52EF10, &CAutomobile_::IsDoorReady_, PATCH_JUMP);
	InjectHook(0x52EF90, &CAutomobile_::IsDoorFullyOpen_, PATCH_JUMP);
	InjectHook(0x52EFD0, &CAutomobile_::IsDoorClosed_, PATCH_JUMP);
	InjectHook(0x52F000, &CAutomobile_::IsDoorMissing_, PATCH_JUMP);
	InjectHook(0x53BF40, &CAutomobile_::RemoveRefsToVehicle_, PATCH_JUMP);
	InjectHook(0x53BC60, &CAutomobile_::BlowUpCar_, PATCH_JUMP);
	InjectHook(0x53BF70, &CAutomobile_::SetUpWheelColModel_, PATCH_JUMP);
	InjectHook(0x53C0E0, &CAutomobile_::BurstTyre_, PATCH_JUMP);
	InjectHook(0x437690, &CAutomobile_::GetHeightAboveRoad_, PATCH_JUMP);
	InjectHook(0x53C450, &CAutomobile_::PlayCarHorn_, PATCH_JUMP);
	InjectHook(0x52F030, &CAutomobile::dmgDrawCarCollidingParticles, PATCH_JUMP);
	InjectHook(0x5353A0, &CAutomobile::ResetSuspension, PATCH_JUMP);
	InjectHook(0x52D210, &CAutomobile::SetupSuspensionLines, PATCH_JUMP);
	InjectHook(0x53E000, &CAutomobile::BlowUpCarsInPath, PATCH_JUMP);
	InjectHook(0x42E220, &CAutomobile::HasCarStoppedBecauseOfLight, PATCH_JUMP);
	InjectHook(0x53D320, &CAutomobile::SetBusDoorTimer, PATCH_JUMP);
	InjectHook(0x53D370, &CAutomobile::ProcessAutoBusDoors, PATCH_JUMP);
	InjectHook(0x535250, &CAutomobile::ProcessSwingingDoor, PATCH_JUMP);
	InjectHook(0x53C240, &CAutomobile::Fix, PATCH_JUMP);
	InjectHook(0x53C310, &CAutomobile::SetupDamageAfterLoad, PATCH_JUMP);
	InjectHook(0x530300, &CAutomobile::SpawnFlyingComponent, PATCH_JUMP);
	InjectHook(0x535320, &CAutomobile::RemoveBonnetInPedCollision, PATCH_JUMP);
	InjectHook(0x5301A0, &CAutomobile::SetPanelDamage, PATCH_JUMP);
	InjectHook(0x530120, &CAutomobile::SetBumperDamage, PATCH_JUMP);
	InjectHook(0x530200, &CAutomobile::SetDoorDamage, PATCH_JUMP);
	InjectHook(0x5300E0, &CAutomobile::SetComponentVisibility, PATCH_JUMP);
	InjectHook(0x52D1B0, &CAutomobile::SetupModelNodes, PATCH_JUMP);
	InjectHook(0x53C420, &CAutomobile::SetTaxiLight, PATCH_JUMP);
	InjectHook(0x53BC40, &CAutomobile::GetAllWheelsOffGround, PATCH_JUMP);
	InjectHook(0x5308C0, &CAutomobile::ReduceHornCounter, PATCH_JUMP);
	InjectHook(0x53C440, &CAutomobile::SetAllTaxiLights, PATCH_JUMP);
ENDPATCHES