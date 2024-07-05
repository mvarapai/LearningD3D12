/*****************************************************************//**
 * \file   d3dgui.h
 * \brief  Manages user interface
 * 
 * \author Mikalai Varapai
 * \date   June 2024
 *********************************************************************/

#pragma once

#include "drawable.h"

class UIElement
{
public:
	virtual void Draw() = 0;
};

class GUI
{
	std::vector<UIElement*> mElementArray;

	// Draws UI objects in their order in array
	void Draw()
	{
		for (UIElement* element : mElementArray)
		{
			element->Draw();
		}
	}
};
