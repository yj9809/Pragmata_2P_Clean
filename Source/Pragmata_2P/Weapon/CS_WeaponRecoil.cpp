// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/CS_WeaponRecoil.h"

UCS_WeaponRecoil::UCS_WeaponRecoil()
{
	//흔들림 지속시간
	OscillationDuration = 0.15f;
	OscillationBlendInTime = 0.05f;
	OscillationBlendOutTime = 0.05f;
	
	//회전 흔들림(pitch)
	RotOscillation.Pitch.Amplitude = 2.5f;
	RotOscillation.Pitch.Frequency = 25.f;
	RotOscillation.Pitch.InitialOffset = EOO_OffsetZero;
	
	//yaw
	RotOscillation.Yaw.Amplitude = 1.f;
	RotOscillation.Yaw.Frequency = 18.f;
	RotOscillation.Yaw.InitialOffset = EOO_OffsetRandom;
	
	//roll
	RotOscillation.Roll.Amplitude = 0.5f;
	RotOscillation.Roll.Frequency = 10.f;
}
