/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Implementation of the NullUI.  Everything else is in the
 * UIWindows or UIMac classes.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 UI MANAGER                               *
 *                                                                          *
 ****************************************************************************/

/**
 * All methods are stubs except this one. 
 * For lightweight components, invalidation is redirected
 * to the nearest native parent, which should know how to paint 
 * this component.
 */
PUBLIC void NullUI::invalidate(Component* c)
{
	Component* target = c;

	while (target != NULL && !target->isNativeParent())
	  target = target->getParent();

	if (target != NULL) {
		ComponentUI* ui = target->getUI();
		ui->invalidate(c);
	}

}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
