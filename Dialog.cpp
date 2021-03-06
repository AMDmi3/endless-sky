/* Dialog.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Dialog.h"

#include "Color.h"
#include "Conversation.h"
#include "FillShader.h"
#include "Font.h"
#include "FontSet.h"
#include "GameData.h"
#include "MapDetailPanel.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Screen.h"
#include "shift.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "UI.h"

#include <cmath>
#include <functional>

using namespace std;

namespace {
	static const int WIDTH = 250;
}



// Dialog that has no callback (information only). In this form, there is
// only an "ok" button, not a "cancel" button.
Dialog::Dialog(const string &text)
{
	Init(text, false);
}



// Mission accept / decline dialog.
Dialog::Dialog(const string &text, PlayerInfo &player, const System *system)
	: intFun(bind(&PlayerInfo::MissionCallback, &player, placeholders::_1)),
	system(system), player(&player)
{
	Init(text, true, true);
}



// Draw this panel.
void Dialog::Draw() const
{
	DrawBackdrop();
	
	const Sprite *top = SpriteSet::Get("ui/dialog top");
	const Sprite *middle = SpriteSet::Get("ui/dialog middle");
	const Sprite *bottom = SpriteSet::Get("ui/dialog bottom");
	const Sprite *cancel = SpriteSet::Get("ui/dialog cancel");
	
	// Get the position of the top of this dialog, and of the text and input.
	Point pos(0., (top->Height() + height * middle->Height() + bottom->Height()) * -.5);
	Point textPos(WIDTH * -.5 + 10., pos.Y() + 20.);
	Point inputPos = Point(0., -70.) - pos;
	
	// Draw the top section of the dialog box.
	pos.Y() += top->Height() * .5;
	SpriteShader::Draw(top, pos);
	pos.Y() += top->Height() * .5;
	
	// The middle section is duplicated depending on how long the text is.
	for(int i = 0; i < height; ++i)
	{
		pos.Y() += middle->Height() * .5;
		SpriteShader::Draw(middle, pos);
		pos.Y() += middle->Height() * .5;
	}
	
	// Draw the bottom section.
	const Font &font = FontSet::Get(14);
	pos.Y() += bottom->Height() * .5;
	SpriteShader::Draw(bottom, pos);
	pos.Y() += bottom->Height() * .5 - 25.;
	
	// Draw the buttons, including optionally the cancel button.
	Color bright = *GameData::Colors().Get("bright");
	Color dim = *GameData::Colors().Get("medium");
	Color back = *GameData::Colors().Get("faint");
	if(canCancel)
	{
		string cancelText = isMission ? "Decline" : "Cancel";
		cancelPos = pos + Point(10., 0.);
		SpriteShader::Draw(cancel, cancelPos);
		Point labelPos(
			cancelPos.X() - .5 * font.Width(cancelText),
			cancelPos.Y() - .5 * font.Height());
		font.Draw(cancelText, labelPos, !okIsActive ? bright : dim);
	}
	string okText = isMission ? "Accept" : "OK";
	okPos = pos + Point(90., 0.);
	Point labelPos(
		okPos.X() - .5 * font.Width(okText),
		okPos.Y() - .5 * font.Height());
	font.Draw(okText, labelPos, okIsActive ? bright : dim);
	
	// Draw the text.
	text.Draw(textPos, dim);
	
	// Draw the input, if any.
	if(!isMission && (intFun || stringFun))
	{
		FillShader::Fill(inputPos, Point(WIDTH - 20., 20.), back);
		
		Point stringPos(
			inputPos.X() - (WIDTH - 20) * .5 + 5.,
			inputPos.Y() - .5 * font.Height());
		font.Draw(input, stringPos, bright);
		
		Point barPos(stringPos.X() + font.Width(input) + 2., inputPos.Y());
		FillShader::Fill(barPos, Point(1., 16.), dim);
	}
}



bool Dialog::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command)
{
	if(key >= ' ' && key <= '~' && !isMission && (intFun || stringFun))
	{
		char c = ((mod & KMOD_SHIFT) ? SHIFT[key] : key);
		if(stringFun)
			input += c;
		// Integer input should not allow leading zeros.
		else if(intFun && c == '0' && !input.empty())
			input += c;
		else if(intFun && c >= '1' && c <= '9')
			input += c;
	}
	else if((key == SDLK_DELETE || key == SDLK_BACKSPACE) && !input.empty())
		input.erase(input.length() - 1);
	else if(key == SDLK_TAB && canCancel)
		okIsActive = !okIsActive;
	else if(key == SDLK_LEFT)
		okIsActive = !canCancel;
	else if(key == SDLK_RIGHT)
		okIsActive = true;
	else if(key == SDLK_RETURN || key == 'a' || key == 'd')
	{
		// Shortcuts for "accept" and "decline."
		if(key == 'a')
			okIsActive = true;
		if(key == 'd')
			okIsActive = false;
		if(okIsActive || isMission)
			DoCallback();
		
		GetUI()->Pop(this);
	}
	else if((key == 'm' || command.Has(Command::MAP)) && system && player)
		GetUI()->Push(new MapDetailPanel(*player, -4, system));
	else
		return false;
	
	return true;
}



bool Dialog::Click(int x, int y)
{
	Point clickPos(x, y);
	
	Point ok = clickPos - okPos;
	if(fabs(ok.X()) < 40. && fabs(ok.Y()) < 20.)
	{
		okIsActive = true;
		return DoKey(SDLK_RETURN);
	}
	
	if(canCancel)
	{
		Point cancel = clickPos - cancelPos;
		if(fabs(cancel.X()) < 40. && fabs(cancel.Y()) < 20.)
		{
			okIsActive = false;
			return DoKey(SDLK_RETURN);
		}
	}
	
	return true;
}



// Common code from all three constructors:
void Dialog::Init(const string &message, bool canCancel, bool isMission)
{
	TrapAllEvents();
	
	this->isMission = isMission;
	this->canCancel = canCancel;
	okIsActive = true;
	
	text.SetAlignment(WrappedText::JUSTIFIED);
	text.SetWrapWidth(WIDTH - 20);
	text.SetFont(FontSet::Get(14));
	
	text.Wrap(message);
	
	// If there is a text input, we need 20 pixels for it and 10 pixels padding.
	height = text.Height() + 20 + 30 * (intFun || stringFun);
	// Determine how many 40-pixel extension panels we need.
	if(height < 80)
		height = 0;
	else
		height = (height - 41) / 40;
}



void Dialog::DoCallback() const
{
	if(isMission)
	{
		if(intFun)
			intFun(okIsActive ? Conversation::ACCEPT : Conversation::DECLINE);
		
		return;
	}
	
	if(intFun)
		intFun(input.empty() ? 0 : stoi(input));
	
	if(stringFun)
		stringFun(input);
	
	if(voidFun)
		voidFun();
}
