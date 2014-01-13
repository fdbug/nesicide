/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2012  Jonathan Liss
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

#pragma once

#include "cqtmfc.h"

class CModSequenceEditor : public CWnd
{
   // Qt interfaces
public: // For some reason MOC doesn't like the protection specification inside DECLARE_DYNAMIC
   
	CModSequenceEditor();
	virtual ~CModSequenceEditor();
	DECLARE_DYNAMIC(CModSequenceEditor)
protected:
	DECLARE_MESSAGE_MAP()
private:
	void EditSequence(CPoint point);

	int m_iSX, m_iSY;
	int m_iLX, m_iLY;

	CInstrumentFDS *m_pInstrument;
public:
	virtual afx_msg void OnPaint();

	void SetInstrument(CInstrumentFDS *pInst);

	BOOL CreateEx(DWORD dwExStyle, LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
};
