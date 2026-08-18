// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");

#include "ExtendedInputActionDecorator.h"

#include "Components/RichTextBlock.h"
#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/SlateWidgetRun.h"
#include "Misc/DefaultValueHelper.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ExtendedCommonUI::InputActionRichText
{
	/**
	 * Displays the current-platform icon for an input action, resolved once at construction.
	 *
	 * The action's owning mapping context may not be applied to the local player yet at the
	 * moment this widget is built (e.g. UMG designer preview, or a menu IMC added later), in
	 * which case there's simply no icon to show yet. Rather than poll for that on every widget,
	 * UExtendedInputActionDecorator listens for UEnhancedInputLocalPlayerSubsystem's
	 * ControlMappingsRebuiltDelegate and calls URichTextBlock::RefreshTextLayout() when it fires,
	 * which discards and recreates every run - including a fresh instance of this widget - giving
	 * it another chance to resolve.
	 */
	class SExtendedInputActionIcon : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SExtendedInputActionIcon) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const FKey& DesignTimeKey, ULocalPlayer* LocalPlayer, UInputAction* InputAction, float IconScale, const FTextBlockStyle& TextStyle)
		{
			bHasIcon = !LocalPlayer
				? ResolveDesignTimeBrush(DesignTimeKey, IconScale, TextStyle)
				: ResolveBrush(LocalPlayer, InputAction, IconScale, TextStyle);
			if (!bHasIcon)
			{
				ChildSlot
				[
					SNullWidget::NullWidget
				];
				return;
			}

			ChildSlot
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(IconSize)
				.WidthOverride(IconSize)
				[
					SNew(SImage)
					.Image(&IconBrush)
					.ColorAndOpacity(TextStyle.ColorAndOpacity)
				]
			];
		}

		bool HasIcon() const { return bHasIcon; }
		float GetIconSize() const { return IconSize; }

	private:
		bool ResolveBrush(ULocalPlayer* LocalPlayer, UInputAction* InputAction, float IconScale, const FTextBlockStyle& TextStyle)
		{
			if (!LocalPlayer || !InputAction || !UCommonInputPlatformSettings::Get())
			{
				return false;
			}

			UCommonInputSubsystem* CommonInputSubsystem = LocalPlayer->GetSubsystem<UCommonInputSubsystem>();
			if (!CommonInputSubsystem)
			{
				return false;
			}

			// Note: the TArray<FKey> overload of TryGetInputBrush only does a normal per-key icon
			// lookup when there's exactly one key - with 2+ it instead requires a chord/combo brush
			// configured for that exact key set (InputBrushKeySets), which isn't set up here. Narrow
			// to a single key first, same as CommonUI::GetIconForEnhancedInputAction does.
			const FKey FirstKey = CommonUI::GetFirstKeyForInputType(LocalPlayer, CommonInputSubsystem->GetCurrentInputType(), InputAction);
			if (!FirstKey.IsValid() || !UCommonInputPlatformSettings::Get()->TryGetInputBrush(IconBrush, FirstKey, CommonInputSubsystem->GetCurrentInputType(), CommonInputSubsystem->GetCurrentGamepadName()))
			{
				return false;
			}

			return FinalizeIconSize(IconScale, TextStyle);
		}

		// Fallback used in the UMG designer/editor preview, where there's no local player and
		// therefore no real binding to resolve - mirrors UCommonActionWidget's DesignTimeKey.
		bool ResolveDesignTimeBrush(const FKey& DesignTimeKey, float IconScale, const FTextBlockStyle& TextStyle)
		{
			if (!DesignTimeKey.IsValid() || !UCommonInputPlatformSettings::Get())
			{
				return false;
			}

			ECommonInputType KeyInputType = ECommonInputType::MouseAndKeyboard;
			if (DesignTimeKey.IsGamepadKey())
			{
				KeyInputType = ECommonInputType::Gamepad;
			}
			else if (DesignTimeKey.IsTouch())
			{
				KeyInputType = ECommonInputType::Touch;
			}

			ECommonInputType Dummy;
			FName DefaultGamepadName;
			FCommonInputBase::GetCurrentPlatformDefaults(Dummy, DefaultGamepadName);

			if (!UCommonInputPlatformSettings::Get()->TryGetInputBrush(IconBrush, TArray<FKey>{ DesignTimeKey }, KeyInputType, DefaultGamepadName))
			{
				return false;
			}

			return FinalizeIconSize(IconScale, TextStyle);
		}

		bool FinalizeIconSize(float IconScale, const FTextBlockStyle& TextStyle)
		{
			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const float MaxTextHeight = (float)FontMeasure->GetMaxCharacterHeight(TextStyle.Font);
			IconSize = FMath::Min(MaxTextHeight * IconScale, IconBrush.ImageSize.Y);
			return true;
		}

		// Owned by this widget so the pointer handed to SImage::Image stays valid for its lifetime.
		FSlateBrush IconBrush;
		float IconSize = 0.0f;
		bool bHasIcon = false;
	};
}

FExtendedInputActionDecorator::FExtendedInputActionDecorator(URichTextBlock& InOwner, const FKey& InDesignTimeKey)
	: Owner(&InOwner)
	, DesignTimeKey(InDesignTimeKey)
{
}

bool FExtendedInputActionDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	return RunParseResult.Name == UE::ExtendedCommonUI::InputActionRichText::ActionMarkupKey;
}

TSharedRef<ISlateRun> FExtendedInputActionDecorator::Create(
	const TSharedRef<class FTextLayout>& TextLayout,
	const FTextRunParseResults& RunParseResult,
	const FString& OriginalText,
	const TSharedRef<FString>& InOutModelText,
	const ISlateStyle* Style)
{
	FTextRange ModelRange;
	ModelRange.BeginIndex = InOutModelText->Len();

	FTextRunInfo RunInfo(RunParseResult.Name, FText::FromString(OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex)));
	for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
	{
		RunInfo.MetaData.Add(Pair.Key, OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
	}

	FTextBlockStyle TextStyle = Owner->GetCurrentDefaultTextStyle();

	TSharedPtr<ISlateRun> SlateRun;
	TSharedPtr<SWidget> DecoratorWidget = CreateInputActionWidget(RunParseResult, OriginalText, TextStyle);
	if (DecoratorWidget.IsValid())
	{
		*InOutModelText += TEXT('​'); // Zero-Width Breaking Space
		ModelRange.EndIndex = InOutModelText->Len();

		const FSlateFontInfo Font = TextStyle.Font;
		const float ShadowOffsetY = FMath::Min(0.0f, TextStyle.ShadowOffset.Y);

		// The icon is usually scaled down relative to the full line height (see IconScale), so
		// naively using the font's ascent as the baseline (as FRichTextDecorator::Create /
		// RichTextBlockImageDecorator do) leaves it hanging low, with its bottom below the text
		// baseline. Center it on the line's full height instead.
		TWeakPtr<UE::ExtendedCommonUI::InputActionRichText::SExtendedInputActionIcon> WeakIconWidget =
			StaticCastSharedPtr<UE::ExtendedCommonUI::InputActionRichText::SExtendedInputActionIcon>(DecoratorWidget);

		TAttribute<int16> GetBaseline = TAttribute<int16>::CreateLambda([Font, ShadowOffsetY, WeakIconWidget]()
			{
				const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				const float Ascent = FontMeasure->GetBaseline(Font) - ShadowOffsetY;
				const float LineHeight = (float)FontMeasure->GetMaxCharacterHeight(Font);
				const float IconSize = WeakIconWidget.IsValid() ? WeakIconWidget.Pin()->GetIconSize() : LineHeight;
				return static_cast<int16>(Ascent - (LineHeight - IconSize) / 2.0f);
			});

		FSlateWidgetRun::FWidgetRunInfo WidgetRunInfo(DecoratorWidget.ToSharedRef(), GetBaseline);
		SlateRun = FSlateWidgetRun::Create(TextLayout, RunInfo, InOutModelText, WidgetRunInfo, ModelRange);
	}
	else
	{
		// Fallback to regular text if no widget created
		*InOutModelText += RunInfo.Content.ToString();
		ModelRange.EndIndex = InOutModelText->Len();
		SlateRun = FSlateTextRun::Create(RunInfo, InOutModelText, TextStyle, ModelRange);
	}

	return SlateRun.ToSharedRef();
}

TSharedPtr<SWidget> FExtendedInputActionDecorator::CreateInputActionWidget(const FTextRunParseResults& RunParseResult, const FString& OriginalText, const FTextBlockStyle& DefaultTextStyle) const
{
	// Get action name from the "name" metadata attribute
	const FTextRange* NameRangePtr = RunParseResult.MetaData.Find(TEXT("name"));
	if (!NameRangePtr || NameRangePtr->Len() == 0)
	{
		return TSharedPtr<SWidget>();
	}

	const FString NameValue = OriginalText.Mid(NameRangePtr->BeginIndex, NameRangePtr->EndIndex - NameRangePtr->BeginIndex);
	const FName ActionFName(*NameValue);

	// Get scale from metadata (default to 0.75f)
	float IconScale = UE::ExtendedCommonUI::InputActionRichText::DefaultIconScale;
	const FTextRange* ScalePtr = RunParseResult.MetaData.Find(UE::ExtendedCommonUI::InputActionRichText::ScaleParam);
	if (ScalePtr && ScalePtr->Len() > 0)
	{
		FString ScaleString = OriginalText.Mid(ScalePtr->BeginIndex, ScalePtr->EndIndex - ScalePtr->BeginIndex);
		FDefaultValueHelper::ParseFloat(ScaleString, IconScale);
	}

	const bool bIsDesignTime = Owner.IsValid() && Owner->IsDesignTime();
	if (bIsDesignTime)
	{
		TSharedRef<UE::ExtendedCommonUI::InputActionRichText::SExtendedInputActionIcon> DesignTimeIconWidget =
			SNew(UE::ExtendedCommonUI::InputActionRichText::SExtendedInputActionIcon, DesignTimeKey, nullptr, nullptr, IconScale, DefaultTextStyle);

		return DesignTimeIconWidget->HasIcon() ? DesignTimeIconWidget : TSharedPtr<SWidget>();
	}

	// Find the input action
	UInputAction* InputAction = FindInputActionByName(ActionFName);
	if (!InputAction)
	{
		return TSharedPtr<SWidget>();
	}

	// Get the local player for input subsystem lookup
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return TSharedPtr<SWidget>();
	}

	TSharedRef<UE::ExtendedCommonUI::InputActionRichText::SExtendedInputActionIcon> IconWidget =
		SNew(UE::ExtendedCommonUI::InputActionRichText::SExtendedInputActionIcon, FKey(), LocalPlayer, InputAction, IconScale, DefaultTextStyle);

	if (!IconWidget->HasIcon())
	{
		// Couldn't resolve an icon (e.g. mapping context not applied to this player yet) - fall
		// back to plain text for now; UExtendedInputActionDecorator will force a re-parse via
		// RefreshTextLayout() once ControlMappingsRebuiltDelegate fires.
		return TSharedPtr<SWidget>();
	}

	return IconWidget;
}

UInputAction* FExtendedInputActionDecorator::FindInputActionByName(const FName& ActionName) const
{
	// Resolve by object name directly rather than searching the local player's currently-active
	// mapping contexts: the action's owning context may not be applied yet (e.g. in the UMG
	// designer preview, or a UI-only action never routed through AddMappingContext), in which
	// case the active mappings would be empty even though the action itself is loaded.
	return FindFirstObject<UInputAction>(*ActionName.ToString(), EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("FExtendedInputActionDecorator::FindInputActionByName"));
}

ULocalPlayer* FExtendedInputActionDecorator::GetLocalPlayer() const
{
	if (!Owner.IsValid())
	{
		return nullptr;
	}

	return Owner->GetOwningLocalPlayer();
}

UExtendedInputActionDecorator::UExtendedInputActionDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<ITextDecorator> UExtendedInputActionDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	if (!InOwner)
	{
		return TSharedPtr<ITextDecorator>();
	}

	CachedOwner = InOwner;
	BindControlMappingsRebuilt(InOwner);

#if WITH_EDITORONLY_DATA
	return FExtendedInputActionDecorator::CreateInstance(*InOwner, DesignTimeKey);
#else // WITH_EDITORONLY_DATA
	return FExtendedInputActionDecorator::CreateInstance(*InOwner);
#endif
}

void UExtendedInputActionDecorator::BeginDestroy()
{
	UnbindControlMappingsRebuilt();
	Super::BeginDestroy();
}

void UExtendedInputActionDecorator::BindControlMappingsRebuilt(URichTextBlock* InOwner)
{
	UnbindControlMappingsRebuilt();

	const ULocalPlayer* LocalPlayer = InOwner->GetOwningLocalPlayer();
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		EnhancedInputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &UExtendedInputActionDecorator::HandleControlMappingsRebuilt);
		BoundSubsystem = EnhancedInputSubsystem;
	}
}

void UExtendedInputActionDecorator::UnbindControlMappingsRebuilt()
{
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = BoundSubsystem.Get())
	{
		EnhancedInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &UExtendedInputActionDecorator::HandleControlMappingsRebuilt);
	}
	BoundSubsystem = nullptr;
}

void UExtendedInputActionDecorator::HandleControlMappingsRebuilt()
{
	if (URichTextBlock* RichTextBlock = CachedOwner.Get())
	{
		RichTextBlock->RefreshTextLayout();
	}
}
