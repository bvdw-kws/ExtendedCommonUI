// Copyright (C) 2024 Van de Walle Bastien
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"

#include "ExtendedInputActionDecorator.generated.h"

class URichTextBlock;
class UInputAction;

namespace UE::ExtendedCommonUI::InputActionRichText
{
	// Markup key for input action: <action name="Jump">
	const FString ActionMarkupKey = TEXT("action");
	// Optional scale parameter: <action name="Jump" scale="0.8">
	const FString ScaleParam = TEXT("scale");
	// Default icon scale
	const float DefaultIconScale = 1.0f;
}

/**
 * Decorator for displaying input action icons in RichTextBlock.
 *
 * Usage in rich text:
 *   <action name="Jump"> - shows the icon for the Jump action
 *   <action name="Fire" scale="0.5"> - shows a smaller icon
 *
 * The decorator supports both Enhanced Input actions and CommonUI input action data tables.
 */
class FExtendedInputActionDecorator : public ITextDecorator
{
public:
	static TSharedRef<FExtendedInputActionDecorator> CreateInstance(URichTextBlock& InOwner, const FKey& InDesignTimeKey = FKey())
	{
		return MakeShareable(new FExtendedInputActionDecorator(InOwner, InDesignTimeKey));
	}

	virtual ~FExtendedInputActionDecorator() {}

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef<FString>& InOutModelText, const ISlateStyle* Style) override;

protected:
	FExtendedInputActionDecorator(URichTextBlock& InOwner, const FKey& InDesignTimeKey);

	/**
	 * Creates a widget that displays the input action icon.
	 * @param RunParseResult - The parsed text run results containing name attribute and metadata
	 * @param DefaultTextStyle - The default text block style
	 * @return A widget displaying the input action icon, or nullptr if not found
	 */
	TSharedPtr<SWidget> CreateInputActionWidget(const FTextRunParseResults& RunParseResult, const FString& OriginalText, const FTextBlockStyle& DefaultTextStyle) const;

	/**
	 * Looks up the input action by name.
	 * @param ActionName - The name of the action to find
	 * @return The input action, or nullptr if not found
	 */
	UInputAction* FindInputActionByName(const FName& ActionName) const;

	/**
	 * Gets the local player from the world context.
	 * @return The local player, or nullptr if not available
	 */
	class ULocalPlayer* GetLocalPlayer() const;

private:
	TWeakObjectPtr<URichTextBlock> Owner;

	// Design-time fallback key, sourced from UExtendedInputActionDecorator::DesignTimeKey (or
	// overridden per-tag via the "key" markup attribute). Only ever used when Owner->IsDesignTime().
	FKey DesignTimeKey;
};

/**
 * Blueprint-assignable wrapper so any URichTextBlock (or subclass) can enable input
 * action icon display just by adding this to its DecoratorClasses array.
 */
UCLASS(MinimalAPI, DisplayName="Input Action")
class UExtendedInputActionDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	UExtendedInputActionDecorator(const FObjectInitializer& ObjectInitializer);

	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;

	virtual void BeginDestroy() override;

#if WITH_EDITORONLY_DATA
	/**
	 * Fallback icon key shown in the UMG designer/editor preview, where there's no local player to
	 * resolve the action's real binding from. Can be overridden per-tag with the "key" markup
	 * attribute, e.g. <action name="Jump" key="Gamepad_FaceButton_Bottom">.
	 */
	UPROPERTY(EditAnywhere, Category = "Input Action")
	FKey DesignTimeKey = EKeys::Gamepad_FaceButton_Bottom;
#endif // WITH_EDITORONLY_DATA

private:
	/**
	 * An action's mapping context may not be applied to the local player yet at the moment the
	 * rich text is first parsed (e.g. UMG designer preview, or a menu IMC added later), so icons
	 * can start out unresolved. Rather than poll every frame, bind to this event and force the
	 * owning RichTextBlock to re-parse once mappings actually change.
	 */
	UFUNCTION()
	void HandleControlMappingsRebuilt();

	void BindControlMappingsRebuilt(URichTextBlock* InOwner);
	void UnbindControlMappingsRebuilt();

	TWeakObjectPtr<URichTextBlock> CachedOwner;
	TWeakObjectPtr<class UEnhancedInputLocalPlayerSubsystem> BoundSubsystem;
};
