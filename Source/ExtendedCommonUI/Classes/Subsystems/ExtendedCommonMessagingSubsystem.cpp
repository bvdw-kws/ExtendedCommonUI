// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#include "ExtendedCommonMessagingSubsystem.h"

#include "CommonLocalPlayer.h"
#include "Messaging/CommonGameDialog.h"
#include "PrimaryGameLayout.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExtendedCommonMessagingSubsystem)

class FSubsystemCollectionBase;

UE_DEFINE_GAMEPLAY_TAG(TAG_UI_LAYER_MODAL, "UI.Layer.Modal");

void UExtendedCommonMessagingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UExtendedCommonMessagingSubsystem::ShowConfirmation(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback)
{
	if (UCommonLocalPlayer* LocalPlayer = GetLocalPlayer<UCommonLocalPlayer>())
	{
		if (UPrimaryGameLayout* RootLayout = LocalPlayer->GetRootUILayout())
		{
			RootLayout->PushWidgetToLayerStackAsync<UCommonGameDialog>(TAG_UI_LAYER_MODAL, true, ConfirmationDialogClass, 
				[DialogDescriptor, ResultCallback](EAsyncWidgetLayerState State, UCommonActivatableWidget* Widget)
			{
				if (UCommonGameDialog* Dialog = Cast<UCommonGameDialog>(Widget))
				{
					if (State == EAsyncWidgetLayerState::Initialize)
					{
						Dialog->SetupDialog(DialogDescriptor, ResultCallback);
					}
				}
			});
		}
	}
}

void UExtendedCommonMessagingSubsystem::ShowError(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback)
{
	if (UCommonLocalPlayer* LocalPlayer = GetLocalPlayer<UCommonLocalPlayer>())
	{
		if (UPrimaryGameLayout* RootLayout = LocalPlayer->GetRootUILayout())
		{
			RootLayout->PushWidgetToLayerStackAsync<UCommonGameDialog>(TAG_UI_LAYER_MODAL, true, ErrorDialogClass, 
				[DialogDescriptor, ResultCallback](EAsyncWidgetLayerState State, UCommonActivatableWidget* Widget) 
			{
				if (UCommonGameDialog* Dialog = Cast<UCommonGameDialog>(Widget))
				{
					if (State == EAsyncWidgetLayerState::Initialize)
					{
						Dialog->SetupDialog(DialogDescriptor, ResultCallback);
					}
				}
			});
		}
	}
}
