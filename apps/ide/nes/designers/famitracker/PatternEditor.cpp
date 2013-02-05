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

//
// This is the pattern editor. This class is not derived from any MFC class.
//

#include "PatternEditor.h"
#include "ui_PatternEditor.h"

#include "famitracker/Settings.h"
#include "famitracker/SoundGen.h"
#include "famitracker/ColorScheme.h"
#include "famitracker/Graphics.h"
#include "famitracker/TrackerChannel.h"
#include "famitracker/MainFrame.h"

#include <QScrollBar>

const int VOL_COLUMN_MAX = 0x10;	// TODO: move this

enum {SCROLL_NONE, SCROLL_UP, SCROLL_DOWN, SCROLL_RIGHT, SCROLL_LEFT};

enum {COLUMN_NOTE, COLUMN_INSTRUMENT, COLUMN_VOLUME, COLUMN_EFF1, COLUMN_EFF2, COLUMN_EFF3, COLUMN_EFF4};

const unsigned int COLUMN_SPACE[] = {
	CHAR_WIDTH * 3 + COLUMN_SPACING,
	CHAR_WIDTH, CHAR_WIDTH + COLUMN_SPACING, 
	CHAR_WIDTH + COLUMN_SPACING,  
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH + COLUMN_SPACING,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH + COLUMN_SPACING,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH + COLUMN_SPACING,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH + COLUMN_SPACING
};

const unsigned int COLUMN_WIDTH[] = {
	CHAR_WIDTH * 3,
	CHAR_WIDTH, CHAR_WIDTH, 
	CHAR_WIDTH,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH,
	CHAR_WIDTH, CHAR_WIDTH, CHAR_WIDTH
};

const unsigned int SELECT_WIDTH[] = {
	CHAR_WIDTH * 3 + COLUMN_SPACING,
	CHAR_WIDTH + COLUMN_SPACING, CHAR_WIDTH,
	CHAR_WIDTH + COLUMN_SPACING,  
	CHAR_WIDTH + COLUMN_SPACING, CHAR_WIDTH, CHAR_WIDTH,
	CHAR_WIDTH + COLUMN_SPACING, CHAR_WIDTH, CHAR_WIDTH,
	CHAR_WIDTH + COLUMN_SPACING, CHAR_WIDTH, CHAR_WIDTH,
	CHAR_WIDTH + COLUMN_SPACING, CHAR_WIDTH, CHAR_WIDTH
};

// Pattern header font
LPCTSTR CPatternView::DEFAULT_HEADER_FONT = _T("Tahoma");

const int CPatternView::DEFAULT_FONT_SIZE = 12;
const int CPatternView::DEFAULT_HEADER_FONT_SIZE = 11;

// Numbers of pixels until selection is initiated
const int CPatternView::SELECT_THRESHOLD = 5;

CPatternEditor::CPatternEditor(QWidget *parent) :
   QWidget(parent),
   ui(new Ui::CPatternEditor)
{
   ui->setupUi(this);
   
   songPatterns = new QWidget();   
   songPatterns->installEventFilter(this);
   songPatterns->setFocusProxy(this);
   songPatterns->setMouseTracking(true);
   
   ui->frame->layout()->addWidget(songPatterns);
   
   m_pDocument = NULL;
   
   m_pBackDC = new CDC(songPatterns);
   
   m_bHasFocus = true;
}

CPatternEditor::~CPatternEditor()
{
   delete ui;
   delete songPatterns;
   delete m_pBackDC;
}

int CPatternView::GetRowAtPoint(int PointY) const
{
	return (PointY - HEADER_HEIGHT) / m_iRowHeight - (m_iVisibleRows / 2) + m_iMiddleRow;
}

int CPatternView::GetChannelAtPoint(int PointX) const
{
	int Offset = 0, StartOffset, Channels = m_pDocument->GetAvailableChannels();
	
	StartOffset = PointX - ROW_COL_WIDTH;
	
	if (StartOffset < 0)
		return -1;

	for (int i = m_iFirstChannel; i < Channels; ++i) {
		Offset += m_iChannelWidths[i];
		if (StartOffset < Offset)
			return i;
	}

	return m_iFirstChannel + m_iChannelsVisible;
}

bool CPatternView::IsSingleChannelSelection() const
{
	// Return true if single channel selection
	//return (m_cpSelStart.m_iChannel == m_cpSelEnd.m_iChannel);
	return m_selection.IsSingleChannel();
}

int CPatternView::GetColumnAtPoint(int PointX) const
{
	const int EFFECT_WIDTH = (CHAR_WIDTH * 3 + COLUMN_SPACING);

	int ColumnOffset, Column = -1, i;
	int Channel = -1;
	int Channels = m_pDocument->GetAvailableChannels();
	int Offset = COLUMN_SPACING - 3;
	int StartOffset = PointX - ROW_COL_WIDTH;

	if (StartOffset < 0)
		return 0;//-1;

	for (i = m_iFirstChannel; i < (signed)m_pDocument->GetAvailableChannels(); ++i) {
		Offset += m_iChannelWidths[i];
		if (StartOffset < Offset) {
			ColumnOffset = m_iChannelWidths[i] - (Offset - StartOffset);
			Channel = i;
			break;
		}
	}

	if (Channel == -1)
		return 0;//-1;

	if (ColumnOffset > m_iChannelWidths[i])
		return m_pDocument->GetChannelColumns(Channel);

	Offset = 0;

	for (i = 0; i < m_pDocument->GetChannelColumns(Channel); ++i) {
		Offset += COLUMN_SPACE[i];
		if (ColumnOffset < Offset) {
			Column = i;
			break;
		}
	}

	if (ColumnOffset > Offset)
		return m_pDocument->GetChannelColumns(Channel);

	return Column;
}

CCursorPos CPatternView::GetCursorAtPoint(CPoint point) const
{
	return CCursorPos(GetRowAtPoint(point.y), GetChannelAtPoint(point.x), GetColumnAtPoint(point.x));
}

void CPatternView::SetWindowSize(int width, int height)
{
	m_iWinWidth = width - 3;
	m_iWinHeight = height - 4;
	m_iVisibleRows = (m_iWinHeight - HEADER_HEIGHT) / m_iRowHeight;
}

void CPatternEditor::resizeEvent(QResizeEvent *event)
{
   // CP: counteract unnecessary math in SetWindowSize...
   int width = event->size().width() + 3;
   int height = event->size().height() + 4;
   SetWindowSize(width,height);
}

void CPatternEditor::on_verticalScrollBar_actionTriggered(int arg1)
{
   // CP: these values don't match Qt apparently...
   switch ( arg1 )
   {
   case QAbstractSlider::SliderSingleStepAdd: 
      arg1 = SB_LINEDOWN;
      break;
   case QAbstractSlider::SliderSingleStepSub: 
      arg1 = SB_LINEUP;
      break;
   case QAbstractSlider::SliderPageStepAdd: 
      arg1 = SB_PAGEDOWN;
      break;
   case QAbstractSlider::SliderPageStepSub: 
      arg1 = SB_PAGEUP;
      break;
   case QAbstractSlider::SliderToMinimum:
      arg1 = SB_TOP;
      break;
   case QAbstractSlider::SliderToMaximum:
      arg1 = SB_BOTTOM;
      break;
   case QAbstractSlider::SliderMove:
      arg1 = SB_THUMBTRACK;
      break;
   }

   OnVScroll(arg1,ui->verticalScrollBar->sliderPosition());
   repaint();
}

void CPatternEditor::on_horizontalScrollBar_actionTriggered(int arg1)
{
   // CP: these values don't match Qt apparently...
   switch ( arg1 )
   {
   case QAbstractSlider::SliderSingleStepAdd: 
      arg1 = SB_LINEDOWN;
      break;
   case QAbstractSlider::SliderSingleStepSub: 
      arg1 = SB_LINEUP;
      break;
   case QAbstractSlider::SliderPageStepAdd: 
      arg1 = SB_PAGEDOWN;
      break;
   case QAbstractSlider::SliderPageStepSub: 
      arg1 = SB_PAGEUP;
      break;
   case QAbstractSlider::SliderToMinimum:
      arg1 = SB_TOP;
      break;
   case QAbstractSlider::SliderToMaximum:
      arg1 = SB_BOTTOM;
      break;
   case QAbstractSlider::SliderMove:
      arg1 = SB_THUMBTRACK;
      break;
   }
   
   OnHScroll(arg1,ui->horizontalScrollBar->sliderPosition());
   repaint();
}

void CPatternEditor::songPatterns_paintEvent(QPaintEvent *event)
{
   // Qt attach to the MFC HLE.
   m_pBackDC->attach();

   qDebug("Force full all the time for now");
   m_bUpdated = true;
   m_bForceFullRedraw = true;
   
   qDebug("CreateBackground here for now...");
   CreateBackground(m_pBackDC,true);
   
   DrawScreen(m_pBackDC);
   
   // Qt detach from the MFC HLE
   m_pBackDC->detach();
}

void CPatternEditor::updateViews(long hint)
{
   songPatterns->repaint();
}

bool CPatternEditor::eventFilter(QObject *object, QEvent *event)
{
   if ( object == songPatterns )
   {
      if ( event->type() == QEvent::Paint )
      {
         QPaintEvent* pe = dynamic_cast<QPaintEvent*>(event);
         
         songPatterns_paintEvent(pe);
         return true;
      }
      if ( event->type() == QEvent::MouseMove )
      {
         QMouseEvent* me = dynamic_cast<QMouseEvent*>(event);
         
         if ( me->buttons() == Qt::LeftButton )
         {
            CPoint point(me->pos());
            OnMouseMove(0,point);
            repaint();
            return true;
         }
      }
      if ( event->type() == QEvent::MouseButtonPress )
      {
         QMouseEvent* me = dynamic_cast<QMouseEvent*>(event);
         
         CPoint point(me->pos());
         if ( me->buttons() == Qt::LeftButton )
         {
            OnMouseDown(point);
         }
         if ( me->buttons() == Qt::RightButton )
         {
            OnMouseRDown(point);
         }
         repaint();
         return true;
      }
      if ( event->type() == QEvent::MouseButtonRelease )
      {
         QMouseEvent* me = dynamic_cast<QMouseEvent*>(event);
         
         CPoint point(me->pos());
         OnMouseUp(point);
         repaint();
         return true;
      }
      if ( event->type() == QEvent::MouseButtonDblClick )
      {
         QMouseEvent* me = dynamic_cast<QMouseEvent*>(event);

         CPoint point(me->pos());
         OnMouseDblClk(point);
         repaint();
         return true;
      }
   }
   return false;
}

void CPatternView::AssignDocument(CFamiTrackerDoc *pDoc,CMainFrame* pView)
{
   // Set a new document and view, reset everything
	m_pDocument = pDoc;
	m_pView = pView;

	// Reset variables
	
	m_cpCursorPos = CCursorPos();

	m_iMiddleRow = 0;
	m_iCurrentFrame = 0;
	m_iFirstChannel = 0;

	m_iPlayFrame = 0;
	m_iPlayRow = 0;
	m_iPlayPatternLength = 0;

	m_bSelecting = false;
	m_bCurrentlySelecting = false;
	m_bDragging = false;
	m_bDragStart = false;

	m_selection.SetStart(CCursorPos());
	m_selection.SetEnd(CCursorPos());

	m_bDrawEntire = true;

	m_iScrolling = SCROLL_NONE;

   // CP: From InitView
   qDebug("No clipboard?");
//   m_iClipBoard = ClipBoard;
	m_bShiftPressed = false;
	m_bControlPressed = false;
	m_bFollowMode = true;
	m_bForceFullRedraw = true;

	m_iHighlight = 4;
	m_iHighlightSecond = 16;

	m_iMouseHoverChan = -1;
//	m_iMouseClickChan = -1;

//	Invalidate(true);

	ResetSelection();

   QObject::connect(m_pDocument,SIGNAL(updateViews(long)),this,SLOT(updateViews(long)));  
}

int CPatternView::GetFrame() const
{
   return m_iCurrentFrame;
}

int CPatternView::GetChannel() const
{
	return m_cpCursorPos.m_iChannel;
}

int CPatternView::GetRow() const
{
	return m_cpCursorPos.m_iRow;
}

int CPatternView::GetColumn() const
{
	return m_cpCursorPos.m_iColumn;
}

int CPatternView::GetPlayFrame() const
{
	return m_iPlayFrame;
}

int CPatternView::GetPlayRow() const
{
	return m_iPlayRow;
}

CSelection CPatternView::GetSelection() const
{
	return m_selection;
}

void CPatternView::SetSelection(CSelection &selection)
{
	m_selection = selection;
	m_bSelecting = true;
}

bool CPatternView::IsSelecting() const
{
	return m_bSelecting;
}

void CPatternView::SelectAllChannel()
{
	m_bSelecting = true;

	m_selection.SetStart(CCursorPos(0, m_cpCursorPos.m_iChannel, 0));
	m_selection.SetEnd(CCursorPos(m_iPatternLength - 1, m_cpCursorPos.m_iChannel, m_pDocument->GetChannelColumns(m_cpCursorPos.m_iChannel) - 1));
}

void CPatternView::SelectAll()
{
	if (!m_bSelecting)
		SelectAllChannel();
	else {
		m_bSelecting = true;
		m_selection.SetStart(CCursorPos());
		m_selection.SetEnd(CCursorPos(m_iPatternLength - 1, m_pDocument->GetAvailableChannels() - 1, m_pDocument->GetChannelColumns(m_pDocument->GetAvailableChannels() - 1) - 1));
	}
}

void CPatternView::Interpolate()
{
	stChanNote NoteData;
	int StartRow = m_selection.GetRowStart();
	int EndRow = m_selection.GetRowEnd();
	float StartVal, EndVal, Delta;
	int i, j, k;
	int Effect;

	if (!m_bSelecting)
		return;

	for (i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); i++) {
		for (k = 0; k < (signed)m_pDocument->GetEffColumns(i) + 2; k++) {
			switch (k) {
				case 0:	// Volume
					if (!IsColumnSelected(COLUMN_VOLUME, i))
						continue;
					m_pDocument->GetNoteData(m_iCurrentFrame, i, StartRow, &NoteData);
					if (NoteData.Vol == 0x10)
						continue;
					StartVal = (float)NoteData.Vol;
					m_pDocument->GetNoteData(m_iCurrentFrame, i, EndRow, &NoteData);
					if (NoteData.Vol == 0x10)
						continue;
					EndVal = (float)NoteData.Vol;
					break;
				case 1:	// Effect 1
				case 2:
				case 3:
				case 4:
					if (!IsColumnSelected(COLUMN_EFF1 + k - 1, i))
						continue;
					m_pDocument->GetNoteData(m_iCurrentFrame, i, StartRow, &NoteData);
					if (NoteData.EffNumber[k - 1] == EF_NONE)
						continue;
					StartVal = (float)NoteData.EffParam[k - 1];
					Effect = NoteData.EffNumber[k - 1];
					m_pDocument->GetNoteData(m_iCurrentFrame, i, EndRow, &NoteData);
					if (NoteData.EffNumber[k - 1] == EF_NONE)
						continue;
					EndVal = (float)NoteData.EffParam[k - 1];
					break;
			}

			Delta = (EndVal - StartVal) / float(EndRow - StartRow);

			for (j = StartRow; j < EndRow; j++) {
				m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &NoteData);
				switch (k) {
					case 0: 
						NoteData.Vol = (int)StartVal; 
						break;
					case 1: 
					case 2: 
					case 3: 
					case 4:
						NoteData.EffNumber[k - 1] = Effect;
						NoteData.EffParam[k - 1] = (int)StartVal; 
						break;
				}
				StartVal += Delta;
				m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &NoteData);
			}
		}
	}
}

void CPatternView::Reverse()
{
	stChanNote ReverseBuffer[MAX_PATTERN_LENGTH];
	stChanNote NoteData;
	int StartRow = m_selection.GetRowStart();
	int EndRow = m_selection.GetRowEnd();
	int i, j, k, m;

	if (!m_bSelecting)
		return;

	for (i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); i++) {
		// Copy the selected rows
		for (j = StartRow, m = 0; j < EndRow + 1; j++, m++) {
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, ReverseBuffer + m);
		}
		// Paste reversed
		for (j = EndRow, m = 0; j > StartRow - 1; j--, m++) {
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &NoteData);
			if (IsColumnSelected(COLUMN_NOTE, i)) {
				NoteData.Note = ReverseBuffer[m].Note;
				NoteData.Octave = ReverseBuffer[m].Octave;
			}
			if (IsColumnSelected(COLUMN_INSTRUMENT, i))
				NoteData.Instrument = ReverseBuffer[m].Instrument;
			if (IsColumnSelected(COLUMN_VOLUME, i))
				NoteData.Vol = ReverseBuffer[m].Vol;
			for (k = 0; k < 4; k++) {
				if (IsColumnSelected(k + COLUMN_EFF1, i)) {
					NoteData.EffNumber[k] = ReverseBuffer[m].EffNumber[k];
					NoteData.EffParam[k] = ReverseBuffer[m].EffParam[k];
				}
			}
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &NoteData);
		}
	}
}

void CPatternView::DragPaste(stClipData *pClipData, CSelection *pDragTarget, bool bMix)
{
	// Paste drag'n'drop selections

	// Set cursor location
	m_cpCursorPos = pDragTarget->m_cpStart;

	if (bMix)
		PasteMix(pClipData);
	else
		Paste(pClipData);

	// Update selection
	m_selection.SetStart(pDragTarget->m_cpStart);
	m_selection.SetEnd(pDragTarget->m_cpEnd);
}

void CPatternView::ExpandPattern()
{
	int Rows = m_selection.GetRowEnd() - m_selection.GetRowStart();
	int StartRow = m_selection.GetRowStart();

	for (int i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); ++i) {
		for (int j = Rows / 2; j >= 0; --j) {
			stChanNote Note;
			m_pDocument->GetNoteData(m_iCurrentFrame, i, StartRow + j, &Note);
			m_pDocument->SetNoteData(m_iCurrentFrame, i, StartRow + j * 2, &Note);
			m_pDocument->ClearRow(m_iCurrentFrame, i, StartRow + j * 2 + 1);
		}
	}
}

void CPatternView::ShrinkPattern()
{
	int Rows = m_selection.GetRowEnd() - m_selection.GetRowStart();
	int StartRow = m_selection.GetRowStart();

	for (int i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); ++i) {
		for (int j = 0; j <= Rows; ++j) {
			if (j <= Rows / 2) {
				stChanNote Note;
				m_pDocument->GetNoteData(m_iCurrentFrame, i, StartRow + j * 2, &Note);
				m_pDocument->SetNoteData(m_iCurrentFrame, i, StartRow + j, &Note);
			}
			else {
				m_pDocument->ClearRow(m_iCurrentFrame, i, StartRow + j);
			}
		}
	}
}

void CPatternView::ReplaceInstrument(int Instrument)
{
	stChanNote Note;

	if (!m_bSelecting)
		return;

	for (int i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); ++i) {
		for (int j = m_selection.GetRowStart(); j <= m_selection.GetRowEnd(); ++j) {
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &Note);
			if (Note.Instrument != MAX_INSTRUMENTS) {
				Note.Instrument = Instrument;
			}
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &Note);
		}
	}
}

void CPatternView::Transpose(int Type)
{
	stChanNote Note;

	int RowStart = m_selection.GetRowStart();
	int RowEnd = m_selection.GetRowEnd();
	int ChanStart = m_selection.GetChanStart();
	int ChanEnd = m_selection.GetChanEnd();

	if (!m_bSelecting) {
		RowStart = m_cpCursorPos.m_iRow;
		RowEnd = m_cpCursorPos.m_iRow;
		ChanStart = m_cpCursorPos.m_iChannel;
		ChanEnd = m_cpCursorPos.m_iChannel;
	}

	for (int i = ChanStart; i <= ChanEnd; i++) {
		if (!IsColumnSelected(COLUMN_NOTE, i))
			continue;
		for (int j = RowStart; j <= RowEnd; j++) {
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &Note);
			switch (Type) {
				case TRANSPOSE_DEC_NOTES:
					if (Note.Note > 0 && Note.Note != HALT && Note.Note != RELEASE) {
						if (Note.Note > 1) 
							Note.Note--;
						else {
							if (Note.Octave > 0) {
								Note.Note = B;
								Note.Octave--;
							}
						}
					}
					break;
				case TRANSPOSE_INC_NOTES:
					if (Note.Note > 0 && Note.Note != HALT && Note.Note != RELEASE) {
						if (Note.Note < B)
							Note.Note++;
						else {
							if (Note.Octave < 7) {
								Note.Note = C;
								Note.Octave++;
							}
						}
					}
					break;
				case TRANSPOSE_DEC_OCTAVES:
					if (Note.Octave > 0) 
						Note.Octave--;
					break;
				case TRANSPOSE_INC_OCTAVES:
					if (Note.Octave < 7)
						Note.Octave++;
					break;
			}
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &Note);
		}
	}
}

void CPatternView::ScrollValues(int Type)
{
	stChanNote Note;

	int RowStart = m_selection.GetRowStart();
	int RowEnd = m_selection.GetRowEnd();
	int ChanStart = m_selection.GetChanStart();
	int ChanEnd = m_selection.GetChanEnd();
	int i, j, k;

	if (!m_bSelecting) {
		RowStart = m_cpCursorPos.m_iRow;
		RowEnd = m_cpCursorPos.m_iRow;
		ChanStart = m_cpCursorPos.m_iChannel;
		ChanEnd = m_cpCursorPos.m_iChannel;
	}

	for (i = ChanStart; i <= ChanEnd; i++) {
		for (k = 1; k < 7; k++) {
			if (!IsColumnSelected(k, i))
				continue;
			for (j = RowStart; j <= RowEnd; j++) {
				m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &Note);
				switch (k) {
					case COLUMN_INSTRUMENT:
						if (Note.Instrument != MAX_INSTRUMENTS) {
							if ((Type < 0 && Note.Instrument > 0) || (Type > 0 && Note.Instrument < MAX_INSTRUMENTS - 1))
								Note.Instrument += Type;
						}
						break;
					case COLUMN_VOLUME:
						if (Note.Vol != 0x10) {
							if ((Type < 0 && Note.Vol > 0) || (Type > 0 && Note.Vol < 0x0F))
								Note.Vol += Type;
						}
						break;
					case COLUMN_EFF1:
					case COLUMN_EFF2:
					case COLUMN_EFF3:
					case COLUMN_EFF4:
						if (Note.EffNumber[k - COLUMN_EFF1] != EF_NONE) {
							if ((Type < 0 && Note.EffParam[k - COLUMN_EFF1] > 0) || (Type > 0 && Note.EffParam[k - COLUMN_EFF1] < 255))
								Note.EffParam[k - COLUMN_EFF1] += Type;
						}
						break;
				}
				m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &Note);
			}
		}
	}
}

// Copy and paste ///////////////////////////////////////////////////////////////////////////////////////////

void CPatternView::CopyEntire(stClipData *pClipData)
{
	pClipData->Channels	= m_pDocument->GetAvailableChannels();
	pClipData->Rows		= m_pDocument->GetPatternLength();

	for (int i = 0; i < pClipData->Channels; ++i) {
		for (int j = 0; j < pClipData->Rows; ++j) {
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &pClipData->Pattern[i][j]);
		}
	}
}

void CPatternView::Copy(stClipData *pClipData)
{
	// Copy selection

	int Channel = 0;

	pClipData->Channels		= m_selection.GetChanEnd() - m_selection.GetChanStart() + 1;
	pClipData->StartColumn	= GetSelectColumn(m_selection.GetColStart());
	pClipData->EndColumn	= GetSelectColumn(m_selection.GetColEnd());
	pClipData->Rows			= m_selection.GetRowEnd() - m_selection.GetRowStart() + 1;

	for (int i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); ++i) {
		if (i < 0 || i >= (signed)m_pDocument->GetAvailableChannels())
			continue;
		
		int Row = 0;

		for (int j = m_selection.GetRowStart(); j <= m_selection.GetRowEnd(); ++j) {
			if (j < 0 || j >= m_iPatternLength)
				continue;

			stChanNote NoteData;
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &NoteData);

			if (IsColumnSelected(COLUMN_NOTE, i)) {
				pClipData->Pattern[Channel][Row].Note = NoteData.Note;
				pClipData->Pattern[Channel][Row].Octave = NoteData.Octave;
			}
			if (IsColumnSelected(COLUMN_INSTRUMENT, i)) {
				pClipData->Pattern[Channel][Row].Instrument = NoteData.Instrument;
			}
			if (IsColumnSelected(COLUMN_VOLUME, i)) {
				pClipData->Pattern[Channel][Row].Vol = NoteData.Vol;
			}
			if (IsColumnSelected(COLUMN_EFF1, i)) {
				pClipData->Pattern[Channel][Row].EffNumber[0] = NoteData.EffNumber[0];
				pClipData->Pattern[Channel][Row].EffParam[0] = NoteData.EffParam[0];
			}
			if (IsColumnSelected(COLUMN_EFF2, i)) {
				pClipData->Pattern[Channel][Row].EffNumber[1] = NoteData.EffNumber[1];
				pClipData->Pattern[Channel][Row].EffParam[1] = NoteData.EffParam[1];
			}
			if (IsColumnSelected(COLUMN_EFF3, i)) {
				pClipData->Pattern[Channel][Row].EffNumber[2] = NoteData.EffNumber[2];
				pClipData->Pattern[Channel][Row].EffParam[2] = NoteData.EffParam[2];
			}
			if (IsColumnSelected(COLUMN_EFF4, i)) {
				pClipData->Pattern[Channel][Row].EffNumber[3] = NoteData.EffNumber[3];
				pClipData->Pattern[Channel][Row].EffParam[3] = NoteData.EffParam[3];
			}

			++Row;
		}
		++Channel;
	}
}

void CPatternView::Cut()
{
	// Cut selection
}

void CPatternView::PasteEntire(stClipData *pClipData)
{
	// Paste entire
	int Channels = m_pDocument->GetAvailableChannels();

	for (int i = 0; i < pClipData->Channels; ++i) {
		for (int j = 0; j < pClipData->Rows; ++j) {
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &pClipData->Pattern[i][j]);
		}
	}
}

void CPatternView::Paste(stClipData *pClipData)
{
	// Paste
	stChanNote NoteData;
	int Channels = m_pDocument->GetAvailableChannels();

	// Special, single channel and effect columns only
	if (pClipData->Channels == 1 && pClipData->StartColumn >= COLUMN_EFF1) {
		for (int j = 0; j < pClipData->Rows; ++j) {
			if ((j + m_cpCursorPos.m_iRow) < 0 || (j + m_cpCursorPos.m_iRow) >= m_iPatternLength)
				continue;

			m_pDocument->GetNoteData(m_iCurrentFrame, m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);

			for (int i = pClipData->StartColumn - COLUMN_EFF1; i < (pClipData->EndColumn - COLUMN_EFF1 + 1); ++i) {
				int Offset = (GetSelectColumn(m_cpCursorPos.m_iColumn) - COLUMN_EFF1) + (i - (pClipData->StartColumn - COLUMN_EFF1));
				if (Offset < 4 && Offset >= 0) {
					NoteData.EffNumber[Offset] = pClipData->Pattern[0][j].EffNumber[i];
					NoteData.EffParam[Offset] = pClipData->Pattern[0][j].EffParam[i];
				}
			}

			m_pDocument->SetNoteData(m_iCurrentFrame, m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);
		}
		return;
	}	

	for (int i = 0; i < pClipData->Channels; ++i) {
		if ((i + m_cpCursorPos.m_iChannel) < 0 || (i + m_cpCursorPos.m_iChannel) >= Channels)
			continue;

		for (int j = 0; j < pClipData->Rows; ++j) {
			if ((j + m_cpCursorPos.m_iRow) < 0 || (j + m_cpCursorPos.m_iRow) >= m_iPatternLength)
				continue;

			m_pDocument->GetNoteData(m_iCurrentFrame, i + m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);

			if ((i != 0 || pClipData->StartColumn <= COLUMN_NOTE) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_NOTE)) {
				NoteData.Note = pClipData->Pattern[i][j].Note;
				NoteData.Octave = pClipData->Pattern[i][j].Octave;
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_INSTRUMENT) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_INSTRUMENT)) {
				NoteData.Instrument = pClipData->Pattern[i][j].Instrument;
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_VOLUME) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_VOLUME)) {
				NoteData.Vol = pClipData->Pattern[i][j].Vol;
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF1) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF1)) {
				NoteData.EffNumber[0] = pClipData->Pattern[i][j].EffNumber[0];
				NoteData.EffParam[0] = pClipData->Pattern[i][j].EffParam[0];
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF2) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF2)) {
				NoteData.EffNumber[1] = pClipData->Pattern[i][j].EffNumber[1];
				NoteData.EffParam[1] = pClipData->Pattern[i][j].EffParam[1];
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF3) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF3)) {
				NoteData.EffNumber[2] = pClipData->Pattern[i][j].EffNumber[2];
				NoteData.EffParam[2] = pClipData->Pattern[i][j].EffParam[2];
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF4) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF4)) {
				NoteData.EffNumber[3] = pClipData->Pattern[i][j].EffNumber[3];
				NoteData.EffParam[3] = pClipData->Pattern[i][j].EffParam[3];
			}

			m_pDocument->SetNoteData(m_iCurrentFrame, i + m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);
		}
	}
}

void CPatternView::PasteMix(stClipData *pClipData)
{
	// Paste and mix
	stChanNote NoteData;

	// Special case, single channel and effect columns only
	if (pClipData->Channels == 1 && pClipData->StartColumn >= COLUMN_EFF1) {
		int EffOffset, Col;

		for (int j = 0; j < pClipData->Rows; ++j) {
			if ((j + m_cpCursorPos.m_iRow) < 0 || (j + m_cpCursorPos.m_iRow) >= m_iPatternLength)
				continue;

			m_pDocument->GetNoteData(m_iCurrentFrame, m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);

			for (int i = pClipData->StartColumn - COLUMN_EFF1; i < (pClipData->EndColumn - COLUMN_EFF1 + 1); ++i) {
				EffOffset = (GetSelectColumn(m_cpCursorPos.m_iColumn) - COLUMN_EFF1) + (i - (pClipData->StartColumn - COLUMN_EFF1));
				Col = i;

				if (EffOffset < 4 && EffOffset >= 0 && pClipData->Pattern[0][j].EffNumber[Col] > EF_NONE && NoteData.EffNumber[EffOffset] == EF_NONE) {
					NoteData.EffNumber[EffOffset] = pClipData->Pattern[0][j].EffNumber[Col];
					NoteData.EffParam[EffOffset] = pClipData->Pattern[0][j].EffParam[Col];
				}
			}
			m_pDocument->SetNoteData(m_iCurrentFrame, m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);
		}
		return;
	}	

	for (int i = 0; i < pClipData->Channels; ++i) {
		if ((i + m_cpCursorPos.m_iChannel) < 0 || (i + m_cpCursorPos.m_iChannel) >= (signed)m_pDocument->GetAvailableChannels())
			continue;

		for (int j = 0; j < pClipData->Rows; ++j) {
			if ((j + m_cpCursorPos.m_iRow) < 0 || (j + m_cpCursorPos.m_iRow) >= m_iPatternLength)
				continue;

			m_pDocument->GetNoteData(m_iCurrentFrame, i + m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);

			// Note & octave
			if ((i != 0 || pClipData->StartColumn <= COLUMN_NOTE) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_NOTE)) {
				if (pClipData->Pattern[i][j].Note > 0 && NoteData.Note == NONE) {
					NoteData.Note = pClipData->Pattern[i][j].Note;
					NoteData.Octave = pClipData->Pattern[i][j].Octave;
				}
			}
			// Instrument
			if ((i != 0 || pClipData->StartColumn <= COLUMN_INSTRUMENT) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_INSTRUMENT)) {
				if (pClipData->Pattern[i][j].Instrument < MAX_INSTRUMENTS && NoteData.Instrument == MAX_INSTRUMENTS) {
					NoteData.Instrument = pClipData->Pattern[i][j].Instrument;
				}
			}
			// Volume
			if ((i != 0 || pClipData->StartColumn <= COLUMN_VOLUME) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_VOLUME)) {
				if (pClipData->Pattern[i][j].Vol < VOL_COLUMN_MAX && NoteData.Vol == VOL_COLUMN_MAX) {
					NoteData.Vol = pClipData->Pattern[i][j].Vol;
				}
			}
			// Effects
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF1) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF1)) {
				if (pClipData->Pattern[i][j].EffNumber[0] != EF_NONE && NoteData.EffNumber[0] == EF_NONE) {
					NoteData.EffNumber[0] = pClipData->Pattern[i][j].EffNumber[0];
					NoteData.EffParam[0] = pClipData->Pattern[i][j].EffParam[0];
				}
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF2) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF2)) {
				if (pClipData->Pattern[i][j].EffNumber[1] != EF_NONE && NoteData.EffNumber[0] == EF_NONE) {
					NoteData.EffNumber[1] = pClipData->Pattern[i][j].EffNumber[1];
					NoteData.EffParam[1] = pClipData->Pattern[i][j].EffParam[1];
				}
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF3) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF3)) {
				if (pClipData->Pattern[i][j].EffNumber[2] != EF_NONE && NoteData.EffNumber[0] == EF_NONE) {
					NoteData.EffNumber[2] = pClipData->Pattern[i][j].EffNumber[2];
					NoteData.EffParam[2] = pClipData->Pattern[i][j].EffParam[2];
				}
			}
			if ((i != 0 || pClipData->StartColumn <= COLUMN_EFF4) && (i != (pClipData->Channels - 1) || pClipData->EndColumn >= COLUMN_EFF4)) {
				if (pClipData->Pattern[i][j].EffNumber[3] != EF_NONE && NoteData.EffNumber[0] == EF_NONE) {
					NoteData.EffNumber[3] = pClipData->Pattern[i][j].EffNumber[3];
					NoteData.EffParam[3] = pClipData->Pattern[i][j].EffParam[3];
				}
			}

			m_pDocument->SetNoteData(m_iCurrentFrame, i + m_cpCursorPos.m_iChannel, j + m_cpCursorPos.m_iRow, &NoteData);
		}
	}
}

void CPatternView::DeleteSelectionRows(CSelection &selection)
{
	// Delete selection including rows
	for (int i = selection.GetChanStart(); i <= selection.GetChanEnd(); ++i) {
		if (i < 0 || i > (signed)m_pDocument->GetAvailableChannels())
			continue;

		for (int j = selection.GetRowStart(); j <= selection.GetRowEnd(); ++j) {
			if (j < 0 || j > m_iPatternLength)
				continue;

			m_pDocument->PullUp(m_iCurrentFrame, i, selection.GetRowStart());
		}
	}
}

void CPatternView::DeleteSelection(CSelection &selection)
{
	// Delete selection
	for (int i = selection.GetChanStart(); i <= selection.GetChanEnd(); ++i) {
		if (i < 0 || i > (signed)m_pDocument->GetAvailableChannels())
			continue;

		for (int j = selection.GetRowStart(); j <= selection.GetRowEnd(); ++j) {
			if (j < 0 || j > m_iPatternLength)
				continue;

			stChanNote NoteData;
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &NoteData);

			if (IsColumnSelected(COLUMN_NOTE, i)) {
				NoteData.Note = 0;
				NoteData.Octave = 0;
			}
			if (IsColumnSelected(COLUMN_INSTRUMENT, i)) {
				NoteData.Instrument = 0x40;
			}
			if (IsColumnSelected(COLUMN_VOLUME, i)) {
				NoteData.Vol = 0x10;
			}
			if (IsColumnSelected(COLUMN_EFF1, i)) {
				NoteData.EffNumber[0] = NoteData.EffParam[0] = 0;
			}
			if (IsColumnSelected(COLUMN_EFF2, i)) {
				NoteData.EffNumber[1] = NoteData.EffParam[1] = 0;
			}
			if (IsColumnSelected(COLUMN_EFF3, i)) {
				NoteData.EffNumber[2] = NoteData.EffParam[2] = 0;
			}
			if (IsColumnSelected(COLUMN_EFF4, i)) {
				NoteData.EffNumber[3] = NoteData.EffParam[3] = 0;
			}
		
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &NoteData);
		}
	}
}

void CPatternView::Delete()
{
	// Delete selection
	for (int i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); ++i) {
		if (i < 0 || i > (signed)m_pDocument->GetAvailableChannels())
			continue;

		for (int j = m_selection.GetRowStart(); j <= m_selection.GetRowEnd(); ++j) {
			if (j < 0 || j > m_iPatternLength)
				continue;

			stChanNote NoteData;
			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &NoteData);

			if (IsColumnSelected(COLUMN_NOTE, i)) {
				NoteData.Note = 0;
				NoteData.Octave = 0;
			}
			if (IsColumnSelected(COLUMN_INSTRUMENT, i)) {
				NoteData.Instrument = 0x40;
			}
			if (IsColumnSelected(COLUMN_VOLUME, i)) {
				NoteData.Vol = 0x10;
			}
			if (IsColumnSelected(COLUMN_EFF1, i)) {
				NoteData.EffNumber[0] = NoteData.EffParam[0] = 0;
			}
			if (IsColumnSelected(COLUMN_EFF2, i)) {
				NoteData.EffNumber[1] = NoteData.EffParam[1] = 0;
			}
			if (IsColumnSelected(COLUMN_EFF3, i)) {
				NoteData.EffNumber[2] = NoteData.EffParam[2] = 0;
			}
			if (IsColumnSelected(COLUMN_EFF4, i)) {
				NoteData.EffNumber[3] = NoteData.EffParam[3] = 0;
			}
		
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &NoteData);
		}
	}
}

void CPatternView::RemoveSelectedNotes()
{
	stChanNote NoteData;

	for (int i = m_selection.GetChanStart(); i <= m_selection.GetChanEnd(); ++i) {
		if (i < 0 || i > (signed)m_pDocument->GetAvailableChannels())
			continue;

		for (int j = m_selection.GetRowStart(); j <= m_selection.GetRowEnd(); ++j) {
			if (j < 0 || j > m_iPatternLength)
				continue;

			m_pDocument->GetNoteData(m_iCurrentFrame, i, j, &NoteData);

			if (IsColumnSelected(COLUMN_NOTE, i)) {
				NoteData.Note = 0;
				NoteData.Octave = 0;
			}
			if (IsColumnSelected(COLUMN_INSTRUMENT, i)) {
				NoteData.Instrument = 0x40;
			}
			if (IsColumnSelected(COLUMN_VOLUME, i)) {
				NoteData.Vol = 0x10;
			}
			if (IsColumnSelected(COLUMN_EFF1, i)) {
				NoteData.EffNumber[0] = 0x0;
				NoteData.EffParam[0] = 0x0;
			}
			if (IsColumnSelected(COLUMN_EFF2, i)) {
				NoteData.EffNumber[1] = 0x0;
				NoteData.EffParam[1] = 0x0;
			}
			if (IsColumnSelected(COLUMN_EFF3, i)) {
				NoteData.EffNumber[2] = 0x0;
				NoteData.EffParam[2] = 0x0;
			}
			if (IsColumnSelected(COLUMN_EFF4, i)) {
				NoteData.EffNumber[3] = 0x0;
				NoteData.EffParam[3] = 0x0;
			}
		
			m_pDocument->SetNoteData(m_iCurrentFrame, i, j, &NoteData);
		}
	}
}

void CPatternView::MoveToRow(int Row)
{
	if (theApp.IsPlaying() && m_bFollowMode)
		return;

	if (Row < 0) {
		if (theApp.GetSettings()->General.bWrapFrames) {
			MoveToFrame(m_iCurrentFrame - 1);
			Row = m_pDocument->GetPatternLength() + Row;
		}
		else {
			if (theApp.GetSettings()->General.bWrapCursor)
				Row = m_iPatternLength - 1;
			else
				Row = 0;
		}
	}
	else if (Row > m_iPatternLength - 1) {
		if (theApp.GetSettings()->General.bWrapFrames) {
			MoveToFrame(m_iCurrentFrame + 1);
			Row = Row - m_iPatternLength;
		}
		else {
			if (theApp.GetSettings()->General.bWrapCursor)
				Row = 0;
			else
				Row = m_iPatternLength - 1;
		}
	}

	if (m_bSelecting)
		m_bForceFullRedraw = true;

	if (Row < 0)
		Row = 0;
	if (Row > (int)m_pDocument->GetPatternLength())
		Row = m_pDocument->GetPatternLength();

	m_cpCursorPos.m_iRow = Row;
}

void CPatternView::MoveToFrame(int Frame)
{
	if (m_iCurrentFrame == Frame)
		return;

	if (Frame < 0) {
		if (theApp.GetSettings()->General.bWrapFrames) {
			Frame = m_pDocument->GetFrameCount() - 1;
		}
		else
			Frame = 0;
	}
	if (Frame > (signed)m_pDocument->GetFrameCount() - 1) {
		if (theApp.GetSettings()->General.bWrapFrames) {
			Frame = 0;
		}
		else
			Frame = (signed)m_pDocument->GetFrameCount() - 1;
	}
	
	if (theApp.IsPlaying() && m_bFollowMode) {
		if (m_iPlayFrame != Frame) {
			m_iPlayFrame = Frame;
			m_iPlayRow = 0;
			m_iPlayPatternLength = GetCurrentPatternLength(m_iPlayFrame);
			theApp.GetSoundGenerator()->ResetTempo();
		}
	}

	m_iCurrentFrame = Frame;
	m_bSelecting = false;
	m_bForceFullRedraw = true;
}

void CPatternView::MoveToChannel(int Channel)
{
	if (Channel < 0) {
		if (theApp.GetSettings()->General.bWrapCursor)
			Channel = m_pDocument->GetAvailableChannels() - 1;
		else
			Channel = 0;
	}
	else if (Channel > (signed)m_pDocument->GetAvailableChannels() - 1) {
		if (theApp.GetSettings()->General.bWrapCursor)
			Channel = 0;
		else
			Channel = m_pDocument->GetAvailableChannels() - 1;
	}
	m_cpCursorPos.m_iChannel = Channel;
	m_cpCursorPos.m_iColumn = 0;
}

void CPatternView::MoveToColumn(int Column)
{
	m_cpCursorPos.m_iColumn = Column;
}

void CPatternView::NextFrame()
{
	MoveToFrame(m_iCurrentFrame + 1);
	m_bSelecting = false;
}

void CPatternView::PreviousFrame()
{
	MoveToFrame(m_iCurrentFrame - 1);
	m_bSelecting = false;
}

bool CPatternView::IsColumnSelected(int Column, int Channel) const
{
	int SelColStart = m_selection.GetColStart();
	int SelColEnd	= m_selection.GetColEnd();
	int SelStart, SelEnd;

	if (Channel > m_selection.GetChanStart() && Channel < m_selection.GetChanEnd())
		return true;

	// 0 = Note (0)
	// 1, 2 = Instrument (1)
	// 3 = Volume (2)
	// 4, 5, 6 = Effect 1 (3)
	// 7, 8, 9 = Effect 1 (4)
	// 10, 11, 12 = Effect 1 (5)
	// 13, 14, 15 = Effect 1 (6)

	switch (SelColStart) {
		case 0:						SelStart = 0; break;
		case 1: case 2:				SelStart = 1; break;
		case 3:						SelStart = 2; break;
		case 4: case 5: case 6:		SelStart = 3; break;
		case 7: case 8: case 9:		SelStart = 4; break;
		case 10: case 11: case 12:	SelStart = 5; break;
		case 13: case 14: case 15:	SelStart = 6; break;
	}

	switch (SelColEnd) {
		case 0:						SelEnd = 0; break;
		case 1: case 2:				SelEnd = 1; break;
		case 3:						SelEnd = 2; break;
		case 4: case 5: case 6:		SelEnd = 3; break;
		case 7: case 8: case 9:		SelEnd = 4; break;
		case 10: case 11: case 12:	SelEnd = 5; break;
		case 13: case 14: case 15:	SelEnd = 6; break;
	}

	
	if (Channel == m_selection.GetChanStart() && Channel == m_selection.GetChanEnd()) {
		if (Column >= SelStart && Column <= SelEnd)
			return true;
	}
	else if (Channel == m_selection.GetChanStart()) {
		if (Column >= SelStart)
			return true;
	}
	else if (Channel == m_selection.GetChanEnd()) {
		if (Column <= SelEnd)
			return true;
	}

	return false;
}

int CPatternView::GetSelectColumn(int Column) const
{
	// Translates cursor column to select column

	switch (Column) {
		case 0:	
			return COLUMN_NOTE;
		case 1: case 2:	
			return COLUMN_INSTRUMENT;
		case 3:	
			return COLUMN_VOLUME;
		case 4: case 5: case 6:	
			return COLUMN_EFF1;
		case 7: case 8: case 9:
			return COLUMN_EFF2;
		case 10: case 11: case 12:
			return COLUMN_EFF3;
		case 13: case 14: case 15:
			return COLUMN_EFF4;
	}

	return 0;
}

void CPatternView::AdjustCursor()
{
	// Adjust cursor and keep it inside the pattern area

	// Frame
	if ((unsigned)m_iCurrentFrame >= m_pDocument->GetFrameCount())
		m_iCurrentFrame = m_pDocument->GetFrameCount() - 1;

	// Follow play cursor
	if (m_bFollowMode && (theApp.IsPlaying() || m_bForcePlayRowUpdate) /*&& !m_bSelecting*/) {
		m_cpCursorPos.m_iRow = m_iPlayRow;
		m_iCurrentFrame = m_iPlayFrame;
		m_bForcePlayRowUpdate = false;
	}

	m_iDrawFrame = m_iCurrentFrame;
	m_iPatternLength = GetCurrentPatternLength(m_iDrawFrame);
	m_iChannels = m_pDocument->GetAvailableChannels();

	if (m_cpCursorPos.m_iRow > m_iPatternLength - 1)
		m_cpCursorPos.m_iRow = m_iPatternLength - 1;

	if (theApp.GetSettings()->General.bFreeCursorEdit) {
		// Adjust if cursor is out of screen
		if (m_iMiddleRow < m_iVisibleRows / 2)
			m_iMiddleRow = m_iVisibleRows / 2;

		int CursorDifference = m_cpCursorPos.m_iRow - m_iMiddleRow;

		// Bottom
		while (CursorDifference >= (m_iVisibleRows / 2) && CursorDifference > 0) {
			// Change these if you want one whole page to scroll instead of single lines
			m_iMiddleRow += 1;
			CursorDifference = (m_cpCursorPos.m_iRow - m_iMiddleRow);
		}

		// Top
		while (-CursorDifference > (m_iVisibleRows / 2) && CursorDifference < 0) {
			m_iMiddleRow -= 1;
			CursorDifference = (m_cpCursorPos.m_iRow - m_iMiddleRow);
		}
	}
	else {
		m_iMiddleRow = m_cpCursorPos.m_iRow;
	}

	if (theApp.GetSettings()->General.bFramePreview) {
		m_iPrevPatternLength = GetCurrentPatternLength(m_iDrawFrame - 1);
		m_iNextPatternLength = GetCurrentPatternLength(m_iDrawFrame + 1);
	}
	else
		m_iPrevPatternLength = m_iNextPatternLength = m_iPatternLength;
}

void CPatternView::AdjustCursorChannel()
{
	if (m_iFirstChannel > m_cpCursorPos.m_iChannel)
		m_iFirstChannel = m_cpCursorPos.m_iChannel;

	int ChannelCount = m_pDocument->GetAvailableChannels();
	bool InvisibleChannels = false;
	int Offset = ROW_COL_WIDTH;

	m_iChannelsVisible = ChannelCount - m_iFirstChannel;
	m_iVisibleWidth = 0;

	for (int i = m_iFirstChannel; i < ChannelCount; i++) {
		Offset += m_iChannelWidths[i];
		m_iVisibleWidth += m_iChannelWidths[i];
		if (Offset >= m_iWinWidth) {
			int VisibleChannels = i - m_iFirstChannel;
			m_iChannelsVisible = VisibleChannels;
			InvisibleChannels = true;
			break;
		}
	}

	while ((m_cpCursorPos.m_iChannel - m_iFirstChannel) > (m_iChannelsVisible - 1)) {
		m_iFirstChannel++;
		// Redraw screen properly
      qDebug("Invalidate??");
//		Invalidate(true);
	}

	if (InvisibleChannels && m_iChannelsVisible < (ChannelCount - m_iFirstChannel)) {
		m_iChannelsVisible++;
	}
}

int CPatternView::GetCurrentPatternLength(int Frame) const
{
	stChanNote Note;
	int i, j, k;
	int Channels = m_pDocument->GetAvailableChannels();
	int PatternLength = m_pDocument->GetPatternLength();	// default length

	if (!theApp.GetSettings()->General.bFramePreview)
		return PatternLength;

	if (Frame < 0 || Frame >= (signed)m_pDocument->GetFrameCount())
		return PatternLength;

	for (j = 0; j < PatternLength; j++) {
		for (i = 0; i < Channels; i++) {
			m_pDocument->GetNoteData(Frame, i, j, &Note);
			for (k = 0; k < (signed)m_pDocument->GetEffColumns(i) + 1; k++) {
				switch (Note.EffNumber[k]) {
					case EF_SKIP:
					case EF_JUMP:
					case EF_HALT:
						return j + 1;
				}
			}
		}
	}

	return PatternLength;
}

void CPatternView::ClearRow(CDC *pDC, int Line) 
{
	int ColBg = m_colEmptyBg;
	int Offset = ROW_COL_WIDTH;

	pDC->SetWindowOrg(0, -HEADER_HEIGHT);	

	for (int i = m_iFirstChannel; i < m_iFirstChannel + m_iChannelsVisible; ++i) {
		pDC->FillSolidRect(Offset, Line * m_iRowHeight, m_iChannelWidths[i] - 1, m_iRowHeight, ColBg);
		Offset += m_iChannelWidths[i];
	}

	// Row number
	pDC->FillSolidRect(1, Line * m_iRowHeight, ROW_COL_WIDTH - 2, m_iRowHeight, ColBg);
}

bool IsInRange(CSelection &sel, int Channel, int Row, int Column)
{
	// Return true if cursor is in range of selection
	if (Row >= sel.GetRowStart() && Row <= sel.GetRowEnd()) {
		if (Channel == sel.GetChanStart() && Channel == sel.GetChanEnd()) {
			return (Column >= sel.GetColStart() && Column <= sel.GetColEnd());
		}
		else if (Channel == sel.GetChanStart() && Channel != sel.GetChanEnd()) {
			return (Column >= sel.GetColStart());
		}
		else if (Channel == sel.GetChanEnd() && Channel != sel.GetChanStart()) {
			return (Column <= sel.GetColEnd());
		}
		else if (Channel >= sel.GetChanStart() && Channel < sel.GetChanEnd()) {
			return true;
		}
	}

	return false;
}

bool IsTopEdge(CSelection &sel, int Channel, int Row, int Column)
{
	return Row == sel.GetRowStart();
}

bool IsBottomEdge(CSelection &sel, int Channel, int Row, int Column)
{
	return Row == sel.GetRowEnd();
}

bool IsLeftEdge(CSelection &sel, int Channel, int Row, int Column)
{
	return Channel == sel.GetChanStart() && Column == sel.GetColStart();
}

bool IsRightEdge(CSelection &sel, int Channel, int Row, int Column)
{
	return Channel == sel.GetChanEnd() && Column == sel.GetColEnd();
}

// Draw a single row
void CPatternView::DrawRow(CDC *pDC, int Row, int Line, int Frame, bool bPreview)
{
	// Row is row from pattern to display
	// Line is (absolute) screen line

	const COLORREF RED_BAR_COLOR  = 0x302080;
	const COLORREF BLUE_BAR_COLOR = 0xA02030;
	const COLORREF GRAY_BAR_COLOR = 0x606060;
	const COLORREF SEL_DRAG_COL	  = 0xA08080;

	const unsigned int PREVIEW_DIM = 70;

	const CSettings *pSettings = theApp.GetSettings();

	COLORREF ColCursor	= theApp.GetSettings()->Appearance.iColCursor;
	COLORREF ColBg		= theApp.GetSettings()->Appearance.iColBackground;
	COLORREF ColHiBg	= theApp.GetSettings()->Appearance.iColBackgroundHilite;
	COLORREF ColHiBg2	= theApp.GetSettings()->Appearance.iColBackgroundHilite2;
	COLORREF ColSelect	= theApp.GetSettings()->Appearance.iColSelection;

	bool bEditMode	= m_pView->GetEditMode();

	int Channels	= m_iFirstChannel + m_iChannelsVisible;
	int Frames		= m_pDocument->GetFrameCount();
	int OffsetX		= ROW_COL_WIDTH;

	stChanNote NoteData;

	// Start at row number column
	pDC->SetWindowOrg(0, -HEADER_HEIGHT);

	if (Frame != m_iCurrentFrame && !theApp.GetSettings()->General.bFramePreview) {
		ClearRow(pDC, Line);
		return;
	}

	// Highlight
	bool bHighlight		  = (m_iHighlight > 0) ? !(Row % m_iHighlight) : false;
	bool bSecondHighlight = (m_iHighlightSecond > 0) ? !(Row % m_iHighlightSecond) : false;

	// Clear
	pDC->FillSolidRect(1, Line * m_iRowHeight, ROW_COL_WIDTH - 2, m_iRowHeight, ColBg);

	if (Frame >= Frames) {
		// Below last frame, clear
		for (int i = m_iFirstChannel; i < Channels; ++i) {
			pDC->SetWindowOrg(-OffsetX, -HEADER_HEIGHT - (signed)Line * m_iRowHeight);
			pDC->FillSolidRect(0, 0, m_iChannelWidths[i] - 1, m_iRowHeight, ColBg);
			OffsetX += m_iChannelWidths[i];
		}
		// TODO: I don't know when this is supposed to be called?
		return;
	}

	COLORREF TextColor;

	if (bSecondHighlight)
		TextColor = theApp.GetSettings()->Appearance.iColPatternTextHilite2;
	else if (bHighlight)
		TextColor = theApp.GetSettings()->Appearance.iColPatternTextHilite;
	else
		TextColor = theApp.GetSettings()->Appearance.iColPatternText;

	if (bPreview) {
		ColHiBg2 = DIM(ColHiBg2, PREVIEW_DIM);
		ColHiBg = DIM(ColHiBg, PREVIEW_DIM);
		ColBg = DIM(ColBg, PREVIEW_DIM);
		TextColor = DIM(TextColor, 70);
	}

	// Draw row number
	pDC->SetTextColor(TextColor);

	CString Text;

	if (theApp.GetSettings()->General.bRowInHex) {
		// Hex display
		Text.Format(_T("%02X"), Row);
		pDC->TextOut(7, Line * m_iRowHeight - 1, Text);
	}
	else {
		// Decimal display
		Text.Format(_T("%03i"), Row); 
		pDC->TextOut(4, Line * m_iRowHeight - 1, Text);
	}
 
	COLORREF BackColor;

	if (bSecondHighlight)
		BackColor = ColHiBg2;	// Highlighted row
	else if (bHighlight)
		BackColor = ColHiBg;	// Highlighted row
	else
		BackColor = ColBg;		// Normal

	if (!bPreview && Row == m_iDrawCursorRow) {
		// Cursor row
		if (!m_bHasFocus)
			BackColor = BLEND(GRAY_BAR_COLOR, BackColor, 50);		// Gray
		else if (bEditMode)
			BackColor = BLEND(RED_BAR_COLOR, BackColor, 80);		// Red
		else
			BackColor = BLEND(BLUE_BAR_COLOR, BackColor, 80);		// Blue
	}

	COLORREF SelectColor = BLEND(ColSelect, BackColor, 80);
	COLORREF DragColor = BLEND(SEL_DRAG_COL, BackColor, 80);

	COLORREF SelectEdgeCol = BLEND(SelectColor, 0xFFFFFF, 70);

	RowColorInfo_t colorInfo;

	colorInfo.Note = TextColor;

	if (bSecondHighlight)
		colorInfo.Back = pSettings->Appearance.iColBackgroundHilite2;
	else if (bHighlight)
		colorInfo.Back = pSettings->Appearance.iColBackgroundHilite;
	else
		colorInfo.Back = pSettings->Appearance.iColBackground;

	colorInfo.Shaded = BLEND(TextColor, colorInfo.Back, 30);

	if (!pSettings->General.bPatternColor) {
		colorInfo.Instrument = colorInfo.Volume = colorInfo.Effect = colorInfo.Note;
	}
	else {
		colorInfo.Instrument = pSettings->Appearance.iColPatternInstrument;
		colorInfo.Volume = pSettings->Appearance.iColPatternVolume;
		colorInfo.Effect = pSettings->Appearance.iColPatternEffect;
	}

	if (bPreview) {
		colorInfo.Shaded	 = BLEND(colorInfo.Shaded, colorInfo.Back, 50);
		colorInfo.Note		 = BLEND(colorInfo.Note, colorInfo.Back, 50);
		colorInfo.Instrument = BLEND(colorInfo.Instrument, colorInfo.Back, 50);
		colorInfo.Volume	 = BLEND(colorInfo.Volume, colorInfo.Back, 50);
		colorInfo.Effect	 = BLEND(colorInfo.Effect, colorInfo.Back, 50);
	}

	// Draw channels
	for (int i = m_iFirstChannel; i < Channels; ++i) {

		m_pDocument->GetNoteData(Frame, i, Row, &NoteData);

		pDC->SetWindowOrg(-OffsetX, -HEADER_HEIGHT - (signed)Line * m_iRowHeight);

		int PosX	 = COLUMN_SPACING;
		int SelStart = COLUMN_SPACING;
		int Columns	 = m_pDocument->GetChannelColumns(i);
		int Width	 = m_iChannelWidths[i] - 1;		// Remove 1, spacing between channels

		if (BackColor == ColBg)
			pDC->FillSolidRect(0, 0, Width, m_iRowHeight, BackColor);
		else
			GradientBar(pDC, 0, 0, Width, m_iRowHeight, BackColor, ColBg);

		if ((!m_bFollowMode /*|| m_bSelecting*/) && Row == m_iPlayRow && Frame == m_iPlayFrame && theApp.IsPlaying()) {
			// Play row
			GradientBar(pDC, 0, 0, Width, m_iRowHeight, ROW_PLAY_COLOR, ColBg);
		}

		// Draw each column
		for (int j = 0; j < Columns; ++j) {

			// Selection
			if (m_bSelecting && !bPreview) {
				if (IsInRange(m_selection, i, Row, j)) {
					pDC->FillSolidRect(SelStart - COLUMN_SPACING, 0, SELECT_WIDTH[j], m_iRowHeight, SelectColor);

					// Outline
					if (IsTopEdge(m_selection, i, Row, j))
						pDC->FillSolidRect(SelStart - COLUMN_SPACING, 0, SELECT_WIDTH[j], 1, SelectEdgeCol);
					if (IsBottomEdge(m_selection, i, Row, j))
						pDC->FillSolidRect(SelStart - COLUMN_SPACING, m_iRowHeight - 1, SELECT_WIDTH[j], 1, SelectEdgeCol);
					if (IsLeftEdge(m_selection, i, Row, j))
						pDC->FillSolidRect(SelStart - COLUMN_SPACING, 0, 1, m_iRowHeight, SelectEdgeCol);
					if (IsRightEdge(m_selection, i, Row, j))
						pDC->FillSolidRect(SelStart - COLUMN_SPACING + SELECT_WIDTH[j] - 1, 0, 1, m_iRowHeight, SelectEdgeCol);
				}
			}

			// Dragging
			if (m_bDragging && !bPreview) {
				if (IsInRange(m_selDrag, i, Row, j)) {
					pDC->FillSolidRect(SelStart - COLUMN_SPACING, 0, SELECT_WIDTH[j], m_iRowHeight, DragColor);
				}
			}

			bool bInvert = false;

			// Draw cursor box
			if (i == m_cpCursorPos.m_iChannel && j == m_cpCursorPos.m_iColumn && Row == m_iDrawCursorRow && !bPreview) {
				GradientBar(pDC, PosX - 2, 0, COLUMN_WIDTH[j], m_iRowHeight, ColCursor, ColBg);
				pDC->Draw3dRect(PosX - 2, 0, COLUMN_WIDTH[j], m_iRowHeight, ColCursor, DIM(ColCursor, 50));
				bInvert = true;
			}

			DrawCell(PosX, j, i, bInvert, &NoteData, pDC, &colorInfo);
			PosX += COLUMN_SPACE[j];
			SelStart += SELECT_WIDTH[j];
		}

		OffsetX += m_iChannelWidths[i];
	}

	if (m_iChannelsVisible < Channels && OffsetX < m_iWinWidth) {
		// TODO: I forgot what this is supposed to do?
		pDC->SetWindowOrg(-OffsetX, -HEADER_HEIGHT - (signed)Line * m_iRowHeight);
		pDC->FillSolidRect(0, 0, m_iPatternWidth - OffsetX + ROW_COL_WIDTH, m_iRowHeight, ColBg);	// TODO: adjust channel width
		OffsetX += m_iChannelWidths[/*i*/ Channels];
	}
}

// Draws a colored character
void CPatternView::DrawChar(int x, int y, TCHAR c, COLORREF Color, CDC *pDC)
{
	pDC->SetTextColor(Color);
	pDC->TextOut(x, y, &c, 1);
//	++m_iCharsDrawn;
}

void CPatternView::DrawCell(int PosX, int Column, int Channel, bool bInvert, stChanNote *pNoteData, CDC *pDC, RowColorInfo_t *pColorInfo)
{
	const char NOTES_A[] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};
	const char NOTES_B[] = {'-', '#', '-', '#', '-', '-', '#', '-', '#', '-', '#', '-'};
	const char NOTES_C[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	const char HEX[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	const CTrackerChannel *pTrackerChannel = m_pDocument->GetChannel(Channel);

	int EffNumber = 0, EffParam;

	switch (Column) {
		case 4: case 5: case 6:
			EffNumber	= pNoteData->EffNumber[0];
			EffParam	= pNoteData->EffParam[0];
			break;
		case 7: case 8: case 9:
			EffNumber	= pNoteData->EffNumber[1];
			EffParam	= pNoteData->EffParam[1];
			break;
		case 10: case 11: case 12:
			EffNumber	= pNoteData->EffNumber[2];
			EffParam	= pNoteData->EffParam[2];
			break;
		case 13: case 14: case 15:
			EffNumber	= pNoteData->EffNumber[3];
			EffParam	= pNoteData->EffParam[3];
			break;
	}

	// Detect invalid note data
	if (pNoteData->Note < 0 || pNoteData->Note > HALT || EffNumber >= EF_COUNT || pNoteData->Instrument > MAX_INSTRUMENTS) {
		if (Column == 0/* || Column == 4*/) {
			CString Text;
			Text.Format(_T("(invalid)"));
			pDC->SetTextColor(RED(255));
			pDC->TextOut(PosX, -1, Text);
		}
		return;
	}

	COLORREF InstColor = pColorInfo->Instrument;

	// Make non-available instruments red in the pattern editor
	if (pNoteData->Instrument < MAX_INSTRUMENTS && (!m_pDocument->IsInstrumentUsed(pNoteData->Instrument))) {
		InstColor = RED(255);
	}

	int PosY = -2;
	PosX -= 1;

#define BAR(x, y) pDC->FillSolidRect((x) + 3, (y) + (m_iRowHeight / 2) + 1, CHAR_WIDTH - 7, 1, pColorInfo->Shaded)

	switch (Column) {
		case 0:
			// Note and octave
			switch (pNoteData->Note) {
				case NONE:
					BAR(PosX, PosY);
					BAR(PosX + CHAR_WIDTH, PosY);
					BAR(PosX + CHAR_WIDTH * 2, PosY);
					return;
				case HALT:
					// Note stop
					GradientBar(pDC, PosX + 5, (m_iRowHeight / 2) - 2, CHAR_WIDTH * 3 - 11, m_iRowHeight / 4, pColorInfo->Note, pColorInfo->Back);
					return;
				case RELEASE:
					// Note release
					pDC->FillSolidRect(PosX + 5, 3, CHAR_WIDTH * 3 - 11, 2, pColorInfo->Note);
					pDC->FillSolidRect(PosX + 5, 7, CHAR_WIDTH * 3 - 11, 2, pColorInfo->Note);
					return;
				default:
					if (pTrackerChannel->GetID() == CHANID_NOISE) {
						// Noise
						char NoiseFreq = (pNoteData->Note - 1 + pNoteData->Octave * 12) & 0x0F;
						DrawChar(PosX, PosY, HEX[NoiseFreq], pColorInfo->Note, pDC);
						DrawChar(PosX + CHAR_WIDTH, PosY, '-', pColorInfo->Note, pDC);
						DrawChar(PosX + CHAR_WIDTH * 2, PosY, '#', pColorInfo->Note, pDC);
					}
					else {
						// The rest
						DrawChar(PosX, PosY, NOTES_A[pNoteData->Note - 1], pColorInfo->Note, pDC);
						DrawChar(PosX + CHAR_WIDTH, PosY, NOTES_B[pNoteData->Note - 1], pColorInfo->Note, pDC);
						DrawChar(PosX + CHAR_WIDTH * 2, PosY, NOTES_C[pNoteData->Octave], pColorInfo->Note, pDC);
					}
					return;
			}
			return;
		case 1:
			// Instrument x0
			if (pNoteData->Instrument == MAX_INSTRUMENTS || pNoteData->Note == HALT || pNoteData->Note == RELEASE)
				BAR(PosX, PosY);
			else
				DrawChar(PosX, PosY, HEX[pNoteData->Instrument >> 4], InstColor, pDC);
			return;
		case 2:
			// Instrument 0x
			if (pNoteData->Instrument == MAX_INSTRUMENTS || pNoteData->Note == HALT || pNoteData->Note == RELEASE)
				BAR(PosX, PosY);
			else
				DrawChar(PosX, PosY, HEX[pNoteData->Instrument & 0x0F], InstColor, pDC);
			return;
		case 3: 
			// Volume
			if (pNoteData->Vol == 0x10 || pTrackerChannel->GetID() == CHANID_DPCM)	// TODO: replace 0x10
				BAR(PosX, PosY);
			else
				DrawChar(PosX, PosY, HEX[pNoteData->Vol & 0x0F], pColorInfo->Volume, pDC);
			return;
		case 4: case 7: case 10: case 13:
			// Effect type
			if (EffNumber == 0)
				BAR(PosX, PosY);
			else {
				DrawChar(PosX, PosY, EFF_CHAR[EffNumber - 1], pColorInfo->Effect, pDC);
			}
			return;
		case 5: case 8: case 11: case 14:
			// Effect param x
			if (EffNumber == 0)
				BAR(PosX, PosY);
			else
				DrawChar(PosX, PosY, HEX[(EffParam >> 4) & 0x0F], pColorInfo->Note, pDC);
			return;
		case 6: case 9: case 12: case 15:
			// Effect param y
			if (EffNumber == 0)
				BAR(PosX, PosY);
			else
				DrawChar(PosX, PosY, HEX[EffParam & 0x0F], pColorInfo->Note, pDC);
			return;
	}
}

void CPatternView::DrawRowArea(CDC *pDC)
{
	// Draw all rows

	COLORREF ColBg = theApp.GetSettings()->Appearance.iColBackground;
	CFont *OldFont = pDC->SelectObject(&m_fontPattern);

	int Row = m_iDrawMiddleRow - m_iVisibleRows / 2;
	int Channels = m_pDocument->GetAvailableChannels();

	pDC->SetBkMode(TRANSPARENT);

	// TODO: call some function recursively to preview more than just two frames

   for (int i = 0; i < m_iVisibleRows; ++i) {
		if (Row >= 0 && Row < m_iPatternLength) {
			DrawRow(pDC, Row, i, m_iDrawFrame, false);
		}
		else if (theApp.GetSettings()->General.bFramePreview) {
			// Next frame
			if (m_iDrawFrame < signed(m_pDocument->GetFrameCount() - 1) && Row >= m_iPatternLength) {
				int PatternRow = (Row - m_iPatternLength);
				if (PatternRow >= 0 && PatternRow < m_iNextPatternLength)
					DrawRow(pDC, PatternRow, i, m_iDrawFrame + 1, true);
				else
					ClearRow(pDC, i);
			}
			// Previous frame
			else if (m_iDrawFrame > 0 && Row < 0) {
				int PatternRow = (m_iPrevPatternLength + Row);
				if (PatternRow >= 0 && PatternRow < m_iPrevPatternLength)
					DrawRow(pDC, PatternRow, i, m_iDrawFrame - 1, true);
				else
					ClearRow(pDC, i);
			}
			else
				ClearRow(pDC, i);
		}
		else {
			ClearRow(pDC, i);
		}
		++Row;
	}

	// Last unvisible row
	ClearRow(pDC, m_iVisibleRows);

	pDC->SetWindowOrg(-ROW_COL_WIDTH, -HEADER_HEIGHT);

	//int iSeparatorCol = BLEND(ColBg, (ColBg ^ 0xFFFFFF), 70);

	// Lines between channels
	int Offset = m_iChannelWidths[m_iFirstChannel];
	
	for (int i = m_iFirstChannel; i < Channels; ++i) {
		pDC->FillSolidRect(Offset - 1, 0, 1, m_iWinHeight - HEADER_CHAN_HEIGHT, /*iSeparatorCol*/ m_colSeparator);
		Offset += m_iChannelWidths[i + 1];
	}

	// First line (after row number column)
	pDC->FillSolidRect(-1, 0, 1, m_iWinHeight - HEADER_CHAN_HEIGHT, m_colSeparator);

	// Restore
	pDC->SetWindowOrg(0, 0);
	pDC->SelectObject(OldFont);
}

void CPatternView::DrawMeters(CDC *pDC)
{
	const COLORREF COL_DARK = 0x808080;//0x485848;
	const COLORREF COL_LIGHT = 0x20F040;
	
	const COLORREF COL_DARK_SHADOW = DIM(COL_DARK, 80);
	const COLORREF COL_LIGHT_SHADOW = DIM(COL_LIGHT, 60);

	const COLORREF DPCM_STATE_COLOR = 0x00404040;

	const int BAR_TOP	 = 5+18 + HEADER_CHAN_START;
	const int BAR_LEFT	 = ROW_COL_WIDTH + 7;
	const int BAR_SIZE	 = 6;
	const int BAR_SPACE	 = 1;
	const int BAR_HEIGHT = 5;

	static COLORREF colors[15];
	static COLORREF colors_dim[15];
	static COLORREF colors_dark[15];
	static COLORREF colors_shadow[15];
	static COLORREF colors_dark_shadow[15];

	static int LastSamplePos, LastDeltaPos;

	if (!m_pDocument)
		return;

	int AvailableChannels = (signed)m_pDocument->GetAvailableChannels();
	int Offset = BAR_LEFT;

	CPoint points[5];
	CFont *pOldFont = pDC->SelectObject(&m_fontHeader);

	if (colors[0] == 0) {
		for (int i = 0; i < 15; i++) {
			// Cache colors
			colors[i] = BLEND(COL_LIGHT, 0x00F0F0, (100 - (i * i) / 3));
			colors_dark[i] = BLEND(COL_DARK, 0x00F0F0, (100 - (i * i) / 3));
			colors_shadow[i] = DIM(colors[i], 60);
			colors_dark_shadow[i] = DIM(colors_dark[i], 70);
			colors_dim[i] = DIM(colors[i], 90);
		}
	}

	for (int c = m_iFirstChannel; c < AvailableChannels; ++c) {
		CTrackerChannel *pChannel = m_pDocument->GetChannel(c);

		for (int i = 0; i < 15; ++i) {
			if (i < pChannel->GetVolumeMeter()) {
				pDC->FillSolidRect(Offset + (i * BAR_SIZE) + BAR_SIZE - 1, BAR_TOP + 1, 1, BAR_HEIGHT, colors_shadow[i]);
				pDC->FillSolidRect(Offset + (i * BAR_SIZE) + 1, BAR_TOP + BAR_HEIGHT, BAR_SIZE - 1, 1, colors_shadow[i]);
				pDC->FillSolidRect(CRect(Offset + (i * BAR_SIZE), BAR_TOP, Offset + (i * BAR_SIZE) + (BAR_SIZE - BAR_SPACE), BAR_TOP + BAR_HEIGHT), colors[i]);
				pDC->Draw3dRect(CRect(Offset + (i * BAR_SIZE), BAR_TOP, Offset + (i * BAR_SIZE) + (BAR_SIZE - BAR_SPACE), BAR_TOP + BAR_HEIGHT), colors[i], colors_dim[i]);
			}
			else {
				pDC->FillSolidRect(Offset + (i * BAR_SIZE) + BAR_SIZE - 1, BAR_TOP + 1, BAR_SPACE, BAR_HEIGHT, COL_DARK_SHADOW);
				pDC->FillSolidRect(Offset + (i * BAR_SIZE) + 1, BAR_TOP + BAR_HEIGHT, BAR_SIZE - 1, 1, COL_DARK_SHADOW);
				pDC->FillSolidRect(CRect(Offset + (i * BAR_SIZE), BAR_TOP, Offset + (i * BAR_SIZE) + (BAR_SIZE - BAR_SPACE), BAR_TOP + BAR_HEIGHT), COL_DARK);
			}
		}

		Offset += m_iChannelWidths[c];
	}

	// DPCM
//	if (m_DPCMState.SamplePos != LastSamplePos || m_DPCMState.DeltaCntr != LastDeltaPos) {
//		if (theApp.GetMainWnd()->GetMenu()->GetMenuState(ID_TRACKER_DPCM, MF_BYCOMMAND) == MF_CHECKED) {
//			CString Text;		   

//			pDC->SetBkMode(TRANSPARENT);
//			pDC->SetBkColor(DPCM_STATE_COLOR);
//			pDC->SetTextColor(DPCM_STATE_COLOR);

//			unsigned int iHeadCol1, iHeadCol2, iHeadCol3;

//			iHeadCol1 = GetSysColor(COLOR_3DFACE);
//			iHeadCol2 = GetSysColor(COLOR_BTNHIGHLIGHT);// 0xFFFFFF;
//			iHeadCol3 = GetSysColor(COLOR_APPWORKSPACE);

//			if (m_pView->GetEditMode())
//				iHeadCol3 = BLEND(iHeadCol3, 0x0000FF, 80);

//			GradientRectTriple(pDC, Offset + 10, 0, 150, HEADER_CHAN_HEIGHT, iHeadCol1, iHeadCol2, iHeadCol3);

//			pDC->FillSolidRect(Offset + 10, 0, 150, 1, STATIC_COLOR_SCHEME.FRAME_LIGTH);
//			pDC->FillSolidRect(Offset + 10, HEADER_CHAN_HEIGHT - 1, 150, 1, STATIC_COLOR_SCHEME.FRAME_DARK);

//			Text.Format(_T("Sample position: %02X"), m_DPCMState.SamplePos);
//			pDC->TextOut(Offset + 20, 3, Text);

//			Text.Format(_T("Delta counter: %02X"), m_DPCMState.DeltaCntr);
//			pDC->TextOut(Offset + 20, 17, Text);

//			LastSamplePos = m_DPCMState.SamplePos;
//			LastDeltaPos = m_DPCMState.DeltaCntr;
//		}
//	}

#ifdef DRAW_REGS
	DrawRegisters(pDC);
#endif

	pDC->SelectObject(pOldFont);
}

void CPatternView::DrawUnbufferedArea(CDC *pDC)
{
	// This part of the surface doesn't contain anything useful
	//

	if (m_iVisibleWidth < m_iWinWidth) {

		int Width = m_iWinWidth - m_iVisibleWidth - ROW_COL_WIDTH - ((m_iPatternLength > 1) ? GetSystemMetrics(SM_CXVSCROLL) : 0) - 1;

		// Channel header background
		GradientRectTriple(pDC, m_iVisibleWidth + ROW_COL_WIDTH, HEADER_CHAN_START, Width, HEADER_CHAN_HEIGHT, m_colHead1, m_colHead2, m_pView->GetEditMode() ? m_colHead4 : m_colHead3);
		pDC->Draw3dRect(m_iVisibleWidth + ROW_COL_WIDTH, HEADER_CHAN_START, Width, HEADER_CHAN_HEIGHT, STATIC_COLOR_SCHEME.FRAME_LIGTH, STATIC_COLOR_SCHEME.FRAME_DARK);

		// The big empty area
		pDC->FillSolidRect(m_iVisibleWidth + ROW_COL_WIDTH, HEADER_HEIGHT, Width, m_iWinHeight - HEADER_HEIGHT, m_colEmptyBg);	
	}
}

void CPatternView::DrawHeader(CDC *pDC)
{
	DrawChannelNames(pDC);
}

void CPatternView::DrawChannelNames(CDC *pDC)
{
	LPCTSTR pChanName;

	unsigned int Offset = ROW_COL_WIDTH;
	unsigned int AvailableChannels = m_pDocument->GetAvailableChannels();

	CPoint ArrowPoints[3];
	CFont *OldFont;

	CBrush HoverBrush((COLORREF)0xFFFFFF);
	CPen HoverPen(PS_SOLID, 1, (COLORREF)0x80A080);

	CBrush BlackBrush((COLORREF)0x505050);
	CPen BlackPen(PS_SOLID, 1, (COLORREF)0x808080);

	CObject *pOldBrush, *pOldPen;

	OldFont = pDC->SelectObject(&m_fontHeader);
	pDC->SetBkMode(TRANSPARENT);

	// Channel header background
	GradientRectTriple(pDC, 0, HEADER_CHAN_START, m_iVisibleWidth + ROW_COL_WIDTH, HEADER_CHAN_HEIGHT, m_colHead1, m_colHead2, m_pView->GetEditMode() ? m_colHead4 : m_colHead3);

	// Corner box
	pDC->Draw3dRect(0, HEADER_CHAN_START, ROW_COL_WIDTH, HEADER_CHAN_HEIGHT, STATIC_COLOR_SCHEME.FRAME_LIGTH, STATIC_COLOR_SCHEME.FRAME_DARK);

	for (unsigned int i = m_iFirstChannel; i < AvailableChannels; ++i) {
		bool bMuted = m_pView->IsChannelMuted(i);

		// Frame
		if (bMuted) {
			GradientRectTriple(pDC, Offset, HEADER_CHAN_START, m_iChannelWidths[i], HEADER_CHAN_HEIGHT, m_colHead1, m_colHead1, m_pView->GetEditMode() ? m_colHead4 : m_colHead3);
			pDC->Draw3dRect(Offset, HEADER_CHAN_START, m_iChannelWidths[i], HEADER_CHAN_HEIGHT, BLEND(STATIC_COLOR_SCHEME.FRAME_LIGTH, STATIC_COLOR_SCHEME.FRAME_DARK, 50), STATIC_COLOR_SCHEME.FRAME_DARK); 
		}
		else
			pDC->Draw3dRect(Offset, HEADER_CHAN_START, m_iChannelWidths[i], HEADER_CHAN_HEIGHT, STATIC_COLOR_SCHEME.FRAME_LIGTH, STATIC_COLOR_SCHEME.FRAME_DARK);

		// Text
		CTrackerChannel *pChannel = m_pDocument->GetChannel(i);
      pChanName = pChannel->GetChannelName();

		COLORREF HeadTextCol = bMuted ? STATIC_COLOR_SCHEME.CHANNEL_MUTED : STATIC_COLOR_SCHEME.CHANNEL_NORMAL;

		// Shadow
		pDC->SetTextColor(BLEND(HeadTextCol, 0x00FFFFFF, 20));
		pDC->TextOut(Offset + 11, HEADER_CHAN_START + 6 + (bMuted ? 1 : 0), pChanName);

		if (m_iMouseHoverChan == i)
			HeadTextCol = BLEND(HeadTextCol, 0x0000FFFF, 80);

		// Foreground
		pDC->SetTextColor(HeadTextCol);
		pDC->TextOut(Offset + 10, HEADER_CHAN_START + 5, pChanName);

		// Effect columns
		pDC->SetTextColor(0x404040);
		if (m_pDocument->GetEffColumns(i) > 0)
			pDC->TextOut(Offset + CHANNEL_WIDTH + COLUMN_SPACING + 2, HEADER_CHAN_START + HEADER_CHAN_HEIGHT - 16, _T("fx2"));
		if (m_pDocument->GetEffColumns(i) > 1)
			pDC->TextOut(Offset + CHANNEL_WIDTH + COLUMN_SPACING * 2 + CHAR_WIDTH * 3 + 2, HEADER_CHAN_START + HEADER_CHAN_HEIGHT - 16, _T("fx3"));
		if (m_pDocument->GetEffColumns(i) > 2)
			pDC->TextOut(Offset + CHANNEL_WIDTH + COLUMN_SPACING * 3 + (CHAR_WIDTH * 2) * 3 + 2, HEADER_CHAN_START + HEADER_CHAN_HEIGHT - 16, _T("fx4"));

		// Arrows for expanding/removing fx columns
		if (m_pDocument->GetEffColumns(i) > 0) {
			ArrowPoints[0].SetPoint(Offset + CHANNEL_WIDTH - 17,	 HEADER_CHAN_START + 6);
			ArrowPoints[1].SetPoint(Offset + CHANNEL_WIDTH - 17,	 HEADER_CHAN_START + 6 + 10);
			ArrowPoints[2].SetPoint(Offset + CHANNEL_WIDTH - 17 - 5, HEADER_CHAN_START + 6 + 5);

			if (m_iMouseHoverChan == i && m_iMouseHoverEffArrow == 1) {
				pOldBrush = pDC->SelectObject(&HoverBrush);
				pOldPen = pDC->SelectObject(&HoverPen);
			}
			else {
				pOldBrush = pDC->SelectObject(&BlackBrush);
				pOldPen = pDC->SelectObject(&BlackPen);
			}

			pDC->Polygon(ArrowPoints, 3);
			pDC->SelectObject(pOldBrush);
		}

		if (m_pDocument->GetEffColumns(i) < (MAX_EFFECT_COLUMNS - 1)) {
			ArrowPoints[0].SetPoint(Offset + CHANNEL_WIDTH - 11,	 HEADER_CHAN_START + 6);
			ArrowPoints[1].SetPoint(Offset + CHANNEL_WIDTH - 11,	 HEADER_CHAN_START + 6 + 10);
			ArrowPoints[2].SetPoint(Offset + CHANNEL_WIDTH - 11 + 5, HEADER_CHAN_START + 6 + 5);

			if (m_iMouseHoverChan == i && m_iMouseHoverEffArrow == 2) {
				pOldBrush = pDC->SelectObject(&HoverBrush);
				pOldPen = pDC->SelectObject(&HoverPen);
			}
			else {
				pOldBrush = pDC->SelectObject(&BlackBrush);
				pOldPen = pDC->SelectObject(&BlackPen);
			}

			pDC->Polygon(ArrowPoints, 3);
			pDC->SelectObject(pOldBrush);
		}

		Offset += m_iChannelWidths[i];
	}

	pDC->SelectObject(OldFont);
	pDC->SelectObject(pOldPen);
}

void CPatternView::DrawScreen(CDC *pDC/*, CFamiTrackerView *pView*/)
{
	static int Redraws = 0;
	static bool WasSelecting;

#ifdef _DEBUG
	LARGE_INTEGER StartTime, EndTime;
	LARGE_INTEGER Freq;
#endif

	bool bFast = false;

	// Return if the back buffer isn't created
	if (!m_pBackDC)
		return;

//	m_iCharsDrawn = 0;

	// Performance checking
#ifdef _DEBUG
	QueryPerformanceCounter(&StartTime);
#endif

	if (m_bUpdated) {
		AdjustCursor();
		AdjustCursorChannel();

		// See if a fast redraw is possible
		if (m_iDrawCursorRow == (m_cpCursorPos.m_iRow - 1) && !m_bSelecting && !WasSelecting && !theApp.GetSettings()->General.bFreeCursorEdit && !m_bErasedBg) {
			bFast = true;
		}

		if (m_bForceFullRedraw)
			bFast = false;

		// Fast scrolling down
		if (bFast) {
			// TODO: Fails when jumping and skipping
			m_iDrawCursorRow = m_cpCursorPos.m_iRow;
			m_iDrawMiddleRow = m_iMiddleRow;
			FastScrollDown(m_pBackDC);
			m_pBackDC->SetWindowOrg(0, 0);
//			m_iFastRedraws++;
		} 
		else {
			// Save these as those could change during drawing
			m_iDrawCursorRow = m_cpCursorPos.m_iRow;
			m_iDrawMiddleRow = m_iMiddleRow;
			PaintEditor();
//			m_iRedraws++;
		}
	}

	m_bForceFullRedraw = false;
	m_bUpdated = false;

	if (bFast)
		// Skip header
		pDC->BitBlt(0, HEADER_HEIGHT, m_iVisibleWidth + ROW_COL_WIDTH, m_iWinHeight - HEADER_HEIGHT, m_pBackDC, 0, HEADER_HEIGHT, SRCCOPY);
	else {
		// Copy the back buffer to screen
		if (m_bDrawEntire)
			DrawUnbufferedArea(pDC);
		
		pDC->BitBlt(0, 0, m_iVisibleWidth + ROW_COL_WIDTH, m_iWinHeight, m_pBackDC, 0, 0, SRCCOPY);

		// Auto-reset this
		m_bDrawEntire = false;
	}

#ifdef _DEBUG
	pDC->SetBkColor(DEFAULT_COLOR_SCHEME.CURSOR);
	pDC->SetTextColor(DEFAULT_COLOR_SCHEME.TEXT_HILITE);
	pDC->TextOut(m_iWinWidth - 57, 42, _T("DEBUG"));
	pDC->TextOut(m_iWinWidth - 57, 62, _T("DEBUG"));
	pDC->TextOut(m_iWinWidth - 57, 82, _T("DEBUG"));
#endif

#ifdef _DEBUG

	QueryPerformanceCounter(&EndTime);
	QueryPerformanceFrequency(&Freq);

	CString Text;
	int PosY = 100;
	pDC->SetTextColor(0xFFFF);
	pDC->SetBkColor(0);
	Text.Format(_T("%i ms"), (__int64(EndTime.QuadPart) - __int64(StartTime.QuadPart)) / (__int64(Freq.QuadPart) / 1000));
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;
	Text.Format(_T("%i redraws"), m_iRedraws);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;
	Text.Format(_T("%i fast redraws"), m_iFastRedraws);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;
	Text.Format(_T("%i erases"), m_iErases);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;
	Text.Format(_T("%i new buffers"), m_iBuffers);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;
	Text.Format(_T("%i chars drawn"), m_iCharsDrawn);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;
	Text.Format(_T("%i rows visible"), m_iVisibleRows);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;

	Text.Format(_T("%i (%i) end sel"), m_selection.m_cpEnd.m_iChannel, m_pDocument->GetAvailableChannels());
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;

	Text.Format(_T("Channels: %i, %i"), m_iFirstChannel, m_iChannelsVisible);
	pDC->TextOut(m_iWinWidth - 150, PosY, Text); PosY += 20;

	PosY += 20;

	Text.Format(_T("Selection channel: %i - %i"), m_selection.m_cpStart.m_iChannel, m_selection.m_cpEnd.m_iChannel);
	pDC->TextOut(m_iWinWidth - 180, PosY, Text); PosY += 20;
	Text.Format(_T("Selection column: %i - %i"), m_selection.m_cpStart.m_iColumn, m_selection.m_cpEnd.m_iColumn);
	pDC->TextOut(m_iWinWidth - 180, PosY, Text); PosY += 20;
	Text.Format(_T("Selection row: %i - %i"), m_selection.m_cpStart.m_iRow, m_selection.m_cpEnd.m_iRow);
	pDC->TextOut(m_iWinWidth - 180, PosY, Text); PosY += 20;

	Text.Format(_T("Window width: %i - %i"), m_iWinWidth, m_iVisibleWidth);
	pDC->TextOut(m_iWinWidth - 180, PosY, Text); PosY += 20;

#else

#ifdef WIP
	// Display the BETA text
	CString Text;
	int line = 0;
	int offset = m_iWinWidth - 160;
	Text.Format(_T("BETA %i (%s)"), VERSION_WIP, __DATE__);
	pDC->SetTextColor(0x00FFFF);
	pDC->SetBkMode(TRANSPARENT);
	pDC->TextOut(offset, m_iWinHeight - 38 - 18 * line++, Text);

#ifndef SVN_BUILD
	Text.Format(_T("Dev build"));
	pDC->TextOut(offset, m_iWinHeight - 38 - 18 * line++, Text);
#endif

#endif

#endif

#ifdef _DEBUG
//	DrawLog(pDC);
#endif

#ifdef _DEBUG
	pDC->DrawText(patternEditorText, CRect(m_iPatternWidth + 50, m_iWinHeight / 2, m_iWinWidth , m_iWinHeight), DT_WORDBREAK);
#endif

	WasSelecting = m_bSelecting;
	m_bErasedBg = false;

	UpdateVerticalScroll();
	UpdateHorizontalScroll();
}

void CPatternView::UpdateVerticalScroll()
{
	// Vertical scroll bar
//	SCROLLINFO si;

//	si.cbSize = sizeof(SCROLLINFO);
//	si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
//	si.nMin = 0;
//	si.nMax = m_iPatternLength + theApp.GetSettings()->General.iPageStepSize - 2;
//	si.nPos = m_iDrawCursorRow;
//	si.nPage = theApp.GetSettings()->General.iPageStepSize;

//	m_pView->SetScrollInfo(SB_VERT, &si);
   ui->verticalScrollBar->setMinimum(0);
   ui->verticalScrollBar->setMaximum(m_iPatternLength + theApp.GetSettings()->General.iPageStepSize - 2 - 3);
   ui->verticalScrollBar->setValue(m_iDrawCursorRow);
   ui->verticalScrollBar->setPageStep(theApp.GetSettings()->General.iPageStepSize);
}

void CPatternView::UpdateHorizontalScroll()
{
	// Horizontal scroll bar
//	SCROLLINFO si;

	int ColumnCount = 0, CurrentColumn = 0;
	int Channels = m_pDocument->GetAvailableChannels();

	// Calculate cursor pos
	for (int i = 0; i < Channels; ++i) {
		if (i == m_cpCursorPos.m_iChannel)
			CurrentColumn = ColumnCount + m_cpCursorPos.m_iColumn;
		ColumnCount += m_pDocument->GetChannelColumns(i);
	}

//	si.cbSize = sizeof(SCROLLINFO);
//	si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
//	si.nMin = 0;
//	si.nMax = ColumnCount + COLUMNS - 2;
//	si.nPos = CurrentColumn;
//	si.nPage = COLUMNS;

//	m_pView->SetScrollInfo(SB_HORZ, &si);
   ui->horizontalScrollBar->setMinimum(0);
   ui->horizontalScrollBar->setMaximum(ColumnCount + COLUMNS - 2 - 6);
   ui->horizontalScrollBar->setValue(CurrentColumn);
   ui->horizontalScrollBar->setPageStep(COLUMNS);
}

void CPatternView::PaintEditor()
{
   // Complete redraw of editor, slow

   // Pattern head
   DrawHeader(m_pBackDC);
   // Pattern area
   DrawRowArea(m_pBackDC);
   // Meters
   DrawMeters(m_pBackDC);
}

void CPatternView::CreateBackground(CDC *pDC, bool bForce)
{
	// Called when the background is erased

	static int LastWidth = 0;
	static int LastHeight = 0;
	static int LastPatternWidth = 0;

	int ColumnCount = 0, CurrentColumn = 0;
	int Channels = m_pDocument->GetAvailableChannels();

//	++m_iErases;
	m_iPatternWidth = 0;

	// Calculate channel widths
	for (int i = 0; i < Channels; ++i) {
		int Width = CHAR_WIDTH * 9 + COLUMN_SPACING * 4 + m_pDocument->GetEffColumns(i) * (3 * CHAR_WIDTH + COLUMN_SPACING);
		m_iChannelWidths[i] = Width + 1;
		m_iColumns[i] = m_pDocument->GetChannelColumns(i) - 1;

		if (i == m_cpCursorPos.m_iChannel) {
			CurrentColumn = ColumnCount + m_cpCursorPos.m_iColumn;
			if (m_cpCursorPos.m_iColumn > m_iColumns[i])
				m_cpCursorPos.m_iColumn = m_iColumns[i];
		}

		ColumnCount += m_pDocument->GetChannelColumns(i);

		m_iPatternWidth += Width + 1;
	}

	AdjustCursorChannel();

	m_iPatternWidth = 0;

	for (int i = 0; i < m_iChannelsVisible; ++i) {
		m_iPatternWidth += m_iChannelWidths[m_iFirstChannel + i];
	}

	// TODO: Fix these variables sometime
	m_iVisibleWidth = m_iPatternWidth;

	m_bErasedBg = true;

	if (LastPatternWidth != m_iPatternWidth || LastWidth != m_iWinWidth || LastHeight != m_iWinHeight)
		bForce = true;

	// Allocate backbuffer area, only if window size or pattern width has changed
	if (bForce) {

		AdjustCursor();

		// Allocate backbuffer
//		SAFE_RELEASE(m_pBackBmp);
//		SAFE_RELEASE(m_pBackDC);

		m_pBackBmp = new CBitmap;
      qDebug("Might need to do something different here..");
//		m_pBackDC = new CDC;

		// Setup dc
//		m_pBackBmp->CreateCompatibleBitmap(pDC, m_iPatternWidth + ROW_COL_WIDTH, m_iWinHeight);
//		m_pBackDC->CreateCompatibleDC(pDC);
//		m_pBackDC->SelectObject(m_pBackBmp);

//		EraseBackground(m_pBackDC);
//		Invalidate(false);

//		m_iBuffers++;

		LastWidth = m_iWinWidth;
		LastHeight = m_iWinHeight;
		LastPatternWidth = m_iPatternWidth;
	}

	// Draw directly on screen
#if 1
	DrawUnbufferedArea(pDC);
#else
	// Gray
	pDC->FillSolidRect(m_iPatternWidth + ROW_COL_WIDTH, 0, m_iWinWidth - m_iPatternWidth - ROW_COL_WIDTH, m_iWinHeight, COLOR_FG);
#endif
}

// TODO: change this to universal scroll up and down any number of steps
void CPatternView::FastScrollDown(CDC *pDC)
{
	CFont *OldFont = pDC->SelectObject(&m_fontPattern);
	pDC->SetBkMode(TRANSPARENT);

	int Top = HEADER_HEIGHT + ((m_iVisibleRows / 2) + 2) * m_iRowHeight;
	int Width = ROW_COL_WIDTH + m_iPatternWidth - 1;
	int Height = (m_iVisibleRows / 2) * m_iRowHeight - 1;
	int Row;

	// Save cursor position
	m_iDrawCursorRow = m_cpCursorPos.m_iRow;

	// Bottom section
	pDC->BitBlt(1, Top - m_iRowHeight, Width, Height - m_iRowHeight + 1, pDC, 1, Top, SRCCOPY); 

	// Top section
	Top = HEADER_HEIGHT + m_iRowHeight;
	pDC->BitBlt(1, Top - m_iRowHeight, Width, Height, pDC, 1, Top, SRCCOPY); 

	// Bottom row
	Row = m_iDrawCursorRow + (m_iVisibleRows / 2) - ((m_iVisibleRows & 1) ? 0 : 1);
	if (Row < m_iPatternLength) 
		DrawRow(pDC, Row, m_iVisibleRows - 1, m_iCurrentFrame, false);
	else if ((Row - m_iPatternLength) < m_iNextPatternLength && unsigned(m_iCurrentFrame) < (m_pDocument->GetFrameCount() - 1))
		DrawRow(pDC, (Row - m_iPatternLength), m_iVisibleRows - 1, m_iCurrentFrame + 1, true);
	else
		ClearRow(pDC, m_iVisibleRows - 1);

	// Middle row
	Row = m_iDrawCursorRow;
	if (Row < m_iPatternLength && Row >= 0)
		DrawRow(pDC, Row, m_iVisibleRows / 2, m_iCurrentFrame, false);

	// Above middle row
	Row = m_iDrawCursorRow - 1;
	if (Row < m_iPatternLength && Row >= 0)
		DrawRow(pDC, Row, m_iVisibleRows / 2 - 1, m_iCurrentFrame, false);

	pDC->SelectObject(OldFont);
   
   UpdateVerticalScroll();
}

void CPatternView::ShiftPressed(bool Pressed)
{
	m_bShiftPressed = Pressed;
}

void CPatternView::ControlPressed(bool Pressed)
{
	m_bControlPressed = Pressed;
}

// Used by scrolling

void CPatternView::ScrollLeft()
{
	if (m_cpCursorPos.m_iColumn > 0)
		m_cpCursorPos.m_iColumn--;
	else {
		if (m_cpCursorPos.m_iChannel > 0) {
			m_cpCursorPos.m_iChannel--;
			m_cpCursorPos.m_iColumn = m_iColumns[m_cpCursorPos.m_iChannel];
		}
		else {
			if (theApp.GetSettings()->General.bWrapCursor) {
				m_cpCursorPos.m_iChannel = m_pDocument->GetAvailableChannels() - 1;
				m_cpCursorPos.m_iColumn = m_iColumns[m_cpCursorPos.m_iChannel];
			}
		}
	}
}

void CPatternView::ScrollRight()
{
	if (m_cpCursorPos.m_iColumn < m_iColumns[m_cpCursorPos.m_iChannel])
		m_cpCursorPos.m_iColumn++;
	else {
		if (m_cpCursorPos.m_iChannel < (signed)m_pDocument->GetAvailableChannels() - 1) {
			m_cpCursorPos.m_iChannel++;
			m_cpCursorPos.m_iColumn = 0;
		}
		else {
			if (theApp.GetSettings()->General.bWrapCursor) {
				m_cpCursorPos.m_iChannel = 0;
				m_cpCursorPos.m_iColumn = 0;
			}
		}
	}
}

void CPatternView::ScrollNextChannel()
{
	MoveToChannel(m_cpCursorPos.m_iChannel + 1);
	m_cpCursorPos.m_iColumn = 0;
}

void CPatternView::ScrollPreviousChannel()
{
	MoveToChannel(m_cpCursorPos.m_iChannel - 1);
	m_cpCursorPos.m_iColumn = 0;
}

// Cursor movement
void CPatternView::MoveDown(int Step)
{
	Step = (Step == 0) ? 1 : Step;
	MoveToRow(m_cpCursorPos.m_iRow + Step);
	UpdateSelection();
}

void CPatternView::MoveUp(int Step)
{
	Step = (Step == 0) ? 1 : Step;
	MoveToRow(m_cpCursorPos.m_iRow - Step);
	UpdateSelection();
}

void CPatternView::MoveLeft()
{
	ScrollLeft();
	UpdateSelection();
}

void CPatternView::MoveRight()
{
	ScrollRight();
	UpdateSelection();
}

void CPatternView::MoveToTop()
{
	MoveToRow(0);
	UpdateSelection();
}

void CPatternView::MoveToBottom()
{
	MoveToRow(m_iPatternLength - 1);
	UpdateSelection();
}

void CPatternView::MovePageUp()
{
	int PageSize = theApp.GetSettings()->General.iPageStepSize;
	MoveToRow(m_cpCursorPos.m_iRow - PageSize);
	UpdateSelection();
}

void CPatternView::MovePageDown()
{
	int PageSize = theApp.GetSettings()->General.iPageStepSize;
	MoveToRow(m_cpCursorPos.m_iRow + PageSize);
	UpdateSelection();
}

void CPatternView::NextChannel()
{
	MoveToChannel(m_cpCursorPos.m_iChannel + 1);
	m_cpCursorPos.m_iColumn = 0;

	m_bCurrentlySelecting = false;
	SetSelectionStart();
}

void CPatternView::PreviousChannel()
{
	MoveToChannel(m_cpCursorPos.m_iChannel - 1);
	m_cpCursorPos.m_iColumn = 0;

	m_bCurrentlySelecting = false;
	SetSelectionStart();
}

void CPatternView::FirstChannel()
{
	MoveToChannel(0);
	m_cpCursorPos.m_iColumn	= 0;
	UpdateSelection();
}

void CPatternView::LastChannel()
{
	MoveToChannel(m_pDocument->GetAvailableChannels() - 1);
	m_cpCursorPos.m_iColumn	= 0;
	UpdateSelection();
}

void CPatternView::MoveChannelLeft()
{
	int Channels = m_pDocument->GetAvailableChannels();

	if (--m_cpCursorPos.m_iChannel < 0)
		m_cpCursorPos.m_iChannel = Channels - 1;

	int Columns = m_pDocument->GetChannelColumns(m_cpCursorPos.m_iChannel) - 1;

	if (Columns < m_cpCursorPos.m_iColumn)
		m_cpCursorPos.m_iColumn = Columns;

	UpdateSelection();
}

void CPatternView::MoveChannelRight()
{
	int Channels = m_pDocument->GetAvailableChannels();

	if (++m_cpCursorPos.m_iChannel > (Channels - 1))
		m_cpCursorPos.m_iChannel = 0;

	int Columns = m_pDocument->GetChannelColumns(m_cpCursorPos.m_iChannel) - 1;

	if (Columns < m_cpCursorPos.m_iColumn)
		m_cpCursorPos.m_iColumn = Columns;

	UpdateSelection();
}

void CPatternView::OnHomeKey()
{
	if (m_bControlPressed || theApp.GetSettings()->General.iEditStyle == EDIT_STYLE1) {
		// Control or FT2 edit style
		MoveToTop();
	}
	else {
		if (GetColumn() != 0)
			MoveToColumn(0);
		else if (GetChannel() != 0)
			MoveToChannel(0);
		else if (GetRow() != 0)
			MoveToRow(0);
	}

	UpdateSelection();
}

void CPatternView::OnEndKey()
{
	int Columns = m_pDocument->GetChannelColumns(GetChannel());

	if (m_bControlPressed || theApp.GetSettings()->General.iEditStyle == EDIT_STYLE1) {
		// Control or FT2 edit style
		MoveToBottom();
	}
	else {
		if (GetColumn() != Columns - 1)
			MoveToColumn(Columns - 1);
		else if (GetChannel() != m_iChannels - 1) {
			MoveToChannel(m_iChannels - 1);
			MoveToColumn(m_pDocument->GetChannelColumns(m_iChannels - 1) - 1);
		}
		else if (GetRow() != m_iPatternLength - 1)
			MoveToRow(m_iPatternLength - 1);
	}

	UpdateSelection();
}

void CPatternView::ClearSelection()
{
	ResetSelection();
}

void CPatternView::ResetSelection()
{
	m_bSelecting = false;
	m_bCurrentlySelecting = false;
	m_selection.SetStart(m_cpCursorPos);

	m_bDragStart = false;
	m_bDragging = false;
}

void CPatternView::SetSelectionStart()
{
	m_cpSelCursor = m_cpCursorPos;
}

void CPatternView::UpdateSelection()
{
	// Call after cursor has moved
	// If shift is not pressed, set selection starting point to current cursor position
	// If shift is pressed, update selection end point

	if (m_bShiftPressed) {

		if (!m_bCurrentlySelecting && m_bSelecting) {
			m_selection.SetStart(m_cpSelCursor);
		}

		m_bCurrentlySelecting = true;
		m_bSelecting = true;
		m_selection.SetEnd(m_cpCursorPos);
	}
	else {
		m_bCurrentlySelecting = false;

		if (theApp.GetSettings()->General.iEditStyle != EDIT_STYLE3 || m_bSelecting == false)
			ResetSelection();

		SetSelectionStart();
	}
}

void CPatternView::ApplyColorScheme()
{
	// The color scheme has changed
	//

	const CSettings *pSettings = theApp.GetSettings();

	LOGFONT LogFont;
	LPCTSTR	FontName = pSettings->General.strFont;
	LPCTSTR	HeaderFace = DEFAULT_HEADER_FONT;

	COLORREF ColBackground = pSettings->Appearance.iColBackground;

//	m_iPatternFontSize = ROW_HEIGHT;

	// Fetch font size
	m_iPatternFontSize = pSettings->General.iFontSize;
	m_iRowHeight = m_iPatternFontSize;

   // Update this
	m_iVisibleRows = (m_iWinHeight - HEADER_HEIGHT) / m_iRowHeight;

	// Create pattern font
	memset(&LogFont, 0, sizeof(LOGFONT));
   memcpy(LogFont.lfFaceName, FontName, strlen(FontName));

   LogFont.lfHeight = -m_iPatternFontSize;
//	LogFont.lfHeight = -MulDiv(12, _dpiY, 96);
	LogFont.lfQuality = DRAFT_QUALITY;
	LogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

	// Remove old font
//	if (m_fontPattern.m_hObject != NULL)
//		m_fontPattern.DeleteObject();

	m_fontPattern.CreateFontIndirect(&LogFont);

	// Create header font
	memset(&LogFont, 0, sizeof(LOGFONT));
	memcpy(LogFont.lfFaceName, HeaderFace, strlen(HeaderFace));

	LogFont.lfHeight = -DEFAULT_HEADER_FONT_SIZE;
	//LogFont.lfWeight = 550;
	LogFont.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;

	// Remove old font
//	if (m_fontHeader.m_hObject != NULL)
//		m_fontHeader.DeleteObject();

	m_fontHeader.CreateFontIndirect(&LogFont);

//	if (m_fontCourierNew.m_hObject == NULL)
		m_fontCourierNew.CreateFont(16, 0, 0, 0, 0, FALSE, FALSE, FALSE, 0, 0, 0, DRAFT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Courier New"));

	// Cache some colors
	m_colSeparator	= BLEND(ColBackground, (ColBackground ^ 0xFFFFFF), 75);
	m_colEmptyBg	= DIM(theApp.GetSettings()->Appearance.iColBackground, 70);

//   QPalette palette;
//   m_colHead1 = RGB(palette.color(QPalette::Window).red(),palette.color(QPalette::Window).green(),palette.color(QPalette::Window).blue());
//   m_colHead2 = RGB(palette.color(QPalette::Button).red(),palette.color(QPalette::Button).green(),palette.color(QPalette::Button).blue());
//   m_colHead3 = RGB(palette.color(QPalette::Window).red(),palette.color(QPalette::Window).green(),palette.color(QPalette::Window).blue());
	m_colHead1 = GetSysColor(COLOR_3DFACE);
	m_colHead2 = GetSysColor(COLOR_BTNHIGHLIGHT);
	m_colHead3 = GetSysColor(COLOR_APPWORKSPACE);
	m_colHead4 = BLEND(m_colHead3, 0x4040F0, 80);
}

/*
 *  Class CCursorPos
 */

CCursorPos::CCursorPos() : m_iRow(0), m_iChannel(0), m_iColumn(0) 
{
}

CCursorPos::CCursorPos(int Row, int Channel, int Column) : m_iRow(Row), m_iChannel(Channel), m_iColumn(Column) 
{
}

const CCursorPos& CCursorPos::operator=(const CCursorPos &pos) 
{
	// Copy position
	m_iRow = pos.m_iRow;
	m_iColumn = pos.m_iColumn;
	m_iChannel = pos.m_iChannel;
	return *this;
}

bool CCursorPos::Invalid() const 
{
	return (m_iRow == -1) || (m_iColumn == -1) || (m_iChannel == -1);
}

/*
 *  Class CSelection
 */

int CSelection::GetRowStart() const 
{
	return (m_cpEnd.m_iRow > m_cpStart.m_iRow ?  m_cpStart.m_iRow : m_cpEnd.m_iRow);
}

int CSelection::GetRowEnd() const 
{
	return (m_cpEnd.m_iRow > m_cpStart.m_iRow ? m_cpEnd.m_iRow : m_cpStart.m_iRow);
}

int CSelection::GetColStart() const 
{
	int Col;
	if (m_cpStart.m_iChannel == m_cpEnd.m_iChannel)
		Col = (m_cpEnd.m_iColumn > m_cpStart.m_iColumn ? m_cpStart.m_iColumn : m_cpEnd.m_iColumn); 
	else if (m_cpEnd.m_iChannel > m_cpStart.m_iChannel)
		Col = m_cpStart.m_iColumn;
	else 
		Col = m_cpEnd.m_iColumn;
	switch (Col) {
		case 2: Col = 1; break;
		case 5: case 6: Col = 4; break;
		case 8: case 9: Col = 7; break;
		case 11: case 12: Col = 10; break;
		case 14: case 15: Col = 13; break;
	}
	return Col;
}

int CSelection::GetColEnd() const 
{
	int Col;
	if (m_cpStart.m_iChannel == m_cpEnd.m_iChannel)
		Col = (m_cpEnd.m_iColumn > m_cpStart.m_iColumn ? m_cpEnd.m_iColumn : m_cpStart.m_iColumn); 
	else if (m_cpEnd.m_iChannel > m_cpStart.m_iChannel)
		Col = m_cpEnd.m_iColumn;
	else
		Col = m_cpStart.m_iColumn;
	switch (Col) {
		case 1: Col = 2; break;					// Instrument
		case 4: case 5: Col = 6; break;			// Eff 1
		case 7: case 8: Col = 9; break;			// Eff 2
		case 10: case 11: Col = 12; break;		// Eff 3
		case 13: case 14: Col = 15; break;		// Eff 4
	}
	return Col;	
}

int CSelection::GetChanStart() const 
{
	return (m_cpEnd.m_iChannel > m_cpStart.m_iChannel ? m_cpStart.m_iChannel : m_cpEnd.m_iChannel); 
}

int CSelection::GetChanEnd() const 
{
	return (m_cpEnd.m_iChannel > m_cpStart.m_iChannel ? m_cpEnd.m_iChannel : m_cpStart.m_iChannel); 
}

bool CSelection::IsWithin(CCursorPos pos) const 
{
	if (pos.m_iRow >= GetRowStart() && pos.m_iRow <= GetRowEnd()) {
		if (pos.m_iChannel == GetChanStart() && pos.m_iChannel == GetChanEnd()) {
			if (pos.m_iColumn >= GetColStart() && pos.m_iColumn <= GetColEnd()) {
				return true;
			}
		}
		else if (pos.m_iChannel == GetChanStart() && pos.m_iChannel != GetChanEnd()) {
			if (pos.m_iColumn >= GetColStart()) {
				return true;
			}
		}
		else if (pos.m_iChannel == GetChanEnd() && pos.m_iChannel != GetChanStart()) {
			if (pos.m_iColumn <= GetColEnd()) {
				return true;
			}
		}
		else if (pos.m_iChannel >= GetChanStart() && pos.m_iChannel < GetChanEnd()) {
			return true;
		}
	}
	return false;
}

bool CSelection::IsSingleChannel() const 
{
	return (m_cpStart.m_iChannel == m_cpEnd.m_iChannel);
}

void CSelection::SetStart(CCursorPos pos) 
{
	m_cpStart = pos;
}

void CSelection::SetEnd(CCursorPos pos) 
{
	m_cpEnd = pos;
}

void CPatternView::OnVScroll(UINT nSBCode, UINT nPos)
{
	int PageSize = theApp.GetSettings()->General.iPageStepSize;

	switch (nSBCode) {
		case SB_LINEDOWN: 
			MoveToRow(m_cpCursorPos.m_iRow + 1);
			break;
		case SB_LINEUP:
			MoveToRow(m_cpCursorPos.m_iRow - 1);
			break;
		case SB_PAGEDOWN:
			MoveToRow(m_cpCursorPos.m_iRow + PageSize);
			break;
		case SB_PAGEUP:
			MoveToRow(m_cpCursorPos.m_iRow - PageSize);
			break;
		case SB_TOP:
			MoveToRow(0);
			break;
		case SB_BOTTOM:	
			MoveToRow(m_iPatternLength - 1);
			break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			MoveToRow(nPos);
			break;
	}

	if (!m_bSelecting)
		ResetSelection();
}

void CPatternView::OnHScroll(UINT nSBCode, UINT nPos)
{
	unsigned int count = 0;
	int Channels = m_pDocument->GetAvailableChannels();

	switch (nSBCode) {
		case SB_LINERIGHT: 
			ScrollRight();
			break;
		case SB_LINELEFT: 
			ScrollLeft();
			break;
		case SB_PAGERIGHT: 
			ScrollNextChannel(); 
			break;
		case SB_PAGELEFT: 
			ScrollPreviousChannel(); 
			break;
		case SB_RIGHT: 
			FirstChannel(); 
			break;
		case SB_LEFT: 
			LastChannel(); 
			break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			for (int i = 0; i < m_iChannels; ++i) {
				for (int j = 0; j < m_pDocument->GetChannelColumns(i); ++j) {
					if (count++ == nPos) {
						MoveToChannel(i);
						MoveToColumn(j);
						if (!m_bSelecting)
							ResetSelection();
						return;
					}
				}
			}
	}

	if (!m_bSelecting)
		ResetSelection();
}

// Mouse routines
#if 0
void CPatternView::DragPaste(stClipData *pClipData, CSelection *pDragTarget, bool bMix)
{
	// Paste drag'n'drop selections

	// Set cursor location
	m_cpCursorPos = pDragTarget->m_cpStart;

	if (bMix)
		PasteMix(pClipData);
	else
		Paste(pClipData);

	// Update selection
	m_selection.SetStart(pDragTarget->m_cpStart);
	m_selection.SetEnd(pDragTarget->m_cpEnd);
}

bool CPatternView::OnMouseHover(UINT nFlags, CPoint point)
{
	bool bRedraw = false;

	if (point.y < HEADER_HEIGHT) {
		int Channel = GetChannelAtPoint(point.x);
		int Column = GetColumnAtPoint(point.x);

		if (Channel < 0 || Channel > (signed)m_pDocument->GetAvailableChannels() - 1) {
			bRedraw = m_iMouseHoverEffArrow != 0;
			m_iMouseHoverEffArrow = 0;
			return bRedraw;
		}

		bRedraw = m_iMouseHoverChan != Channel;
		m_iMouseHoverChan = Channel;

		if (Column == 5) {
			if (m_pDocument->GetEffColumns(Channel) > 0) {
				bRedraw = m_iMouseHoverEffArrow != 1;
				m_iMouseHoverEffArrow = 1;
			}
		}
		else if (Column == 6) {
			if (m_pDocument->GetEffColumns(Channel) < (MAX_EFFECT_COLUMNS - 1)) {
				bRedraw = m_iMouseHoverEffArrow != 2;
				m_iMouseHoverEffArrow = 2;
			}
		}
		else {
			bRedraw = m_iMouseHoverEffArrow != 0 || bRedraw;
			m_iMouseHoverEffArrow = 0;
		}
	}
	else {
		bRedraw = (m_iMouseHoverEffArrow != 0) || (m_iMouseHoverChan != -1);
		m_iMouseHoverChan = -1;
		m_iMouseHoverEffArrow = 0;
	}

	return bRedraw;
}

bool CPatternView::OnMouseNcMove()
{
	bool bRedraw = (m_iMouseHoverEffArrow != 0) || (m_iMouseHoverChan != -1);
	m_iMouseHoverEffArrow = 0;
	m_iMouseHoverChan = -1;
	return bRedraw;	
}

void CPatternView::OnMouseScroll(int Delta)
{
	if (theApp.IsPlaying() && m_bFollowMode)
		return;

	if (Delta != 0) {
		
		int ScrollLength;
		if (Delta < 0)
			ScrollLength = theApp.GetSettings()->General.iPageStepSize;
		else
			ScrollLength = -theApp.GetSettings()->General.iPageStepSize;

		m_cpCursorPos.m_iRow += ScrollLength;

		if (m_cpCursorPos.m_iRow > (m_iPatternLength - 1)) {
			if (theApp.GetSettings()->General.bWrapFrames) {
				m_cpCursorPos.m_iRow %= m_iPatternLength;
				MoveToFrame(m_iCurrentFrame + 1);
			}
			else
				m_cpCursorPos.m_iRow = (m_iPatternLength - 1);
		}
		else if (m_cpCursorPos.m_iRow < 0) {
			if (theApp.GetSettings()->General.bWrapFrames && m_iPrevPatternLength > 0) {
				m_cpCursorPos.m_iRow = abs((m_cpCursorPos.m_iRow + m_iPrevPatternLength) % m_iPrevPatternLength);
				MoveToFrame(m_iCurrentFrame - 1);
			}
			else
				m_cpCursorPos.m_iRow = 0;
		}

		m_iMiddleRow = m_cpCursorPos.m_iRow;
	}
}
#endif

void CPatternView::OnMouseDown(CPoint point)
{
   qDebug("OnMouseDown: (%d,%d)",point.x,point.y);
	m_bSelectionInvalid = true;
	m_bSelectedAll = false;

	if (point.y < HEADER_HEIGHT) {
		// Channel headers
		int Channel = GetChannelAtPoint(point.x);
		int Column = GetColumnAtPoint(point.x);

		if (Channel < 0 || Channel > (signed)m_pDocument->GetAvailableChannels() - 1) {
			// Outside of the channel area
			m_pView->UnmuteAllChannels();
			return;
		}

//		m_iMouseClickChan = Channel;

		// Mute/unmute
		if (Column < 5) {
			m_pView->ToggleChannel(Channel);
		}
		// Remove one track effect column
		else if (Column == 5) {
			int Columns = m_pDocument->GetEffColumns(Channel);
			if (Columns > 0) {
				m_pDocument->SetEffColumns(Channel, Columns - 1);
				m_pDocument->UpdateAllViews(NULL, CHANGED_ERASE);
			}
		}
		// Add one track effect column
		else if (Column == 6) {
			int Columns = m_pDocument->GetEffColumns(Channel);
			if (Columns < (MAX_EFFECT_COLUMNS - 1)) {
				m_pDocument->SetEffColumns(Channel, Columns + 1);
				m_pDocument->UpdateAllViews(NULL, CHANGED_ERASE);
			}
		}
	}
	else if (point.y > HEADER_HEIGHT) {
		// Pattern area
		CCursorPos PointPos = GetCursorAtPoint(point);

		if (m_bShiftPressed && !m_selection.IsWithin(PointPos)) {
			// Expand selection
			if (!m_bSelecting) {
				m_selection.SetStart(m_cpCursorPos);
			}
			m_selection.SetEnd(PointPos);
			m_bSelecting = true;
			m_bSelectionInvalid = true;
			m_bFullRowSelect = false;
			m_ptSelStartPoint = point;
		}
		else {
			if (point.x < (m_iPatternWidth + ROW_COL_WIDTH)) {
				// Pattern area
				if (point.x < ROW_COL_WIDTH) {
					m_selection.SetStart(CCursorPos(PointPos.m_iRow, 0, 0));
					m_selection.SetEnd(CCursorPos(PointPos.m_iRow, m_pDocument->GetAvailableChannels(), m_pDocument->GetChannelColumns(m_pDocument->GetAvailableChannels())));
					m_bSelecting = false;
					m_bSelectionInvalid = false;
					m_bFullRowSelect = true;
					m_ptSelStartPoint = point;
					return;
				}
				else
					m_bFullRowSelect = false;

				if (PointPos.m_iChannel < 0 || PointPos.m_iChannel > (signed)m_pDocument->GetAvailableChannels() - 1)
					return;
				if (PointPos.m_iRow < 0)
					return;
				if (PointPos.m_iColumn < 0)
					return;

				if (m_bSelecting) {
					if (PointPos.m_iRow >= m_selection.GetRowStart() && PointPos.m_iRow <= m_selection.GetRowEnd() && PointPos.m_iChannel >= m_selection.GetChanStart() && PointPos.m_iChannel <= m_selection.GetChanEnd()) {
						if (m_selection.GetChanStart() == m_selection.GetChanEnd()) {
							if (PointPos.m_iChannel == m_selection.GetChanStart() && (PointPos.m_iColumn >= m_selection.GetColStart() && PointPos.m_iColumn <= m_selection.GetColEnd()))
								m_bDragStart = true;
							else {
								m_bDragStart = false;
								m_bDragging = false;
								m_bSelecting = false;
							}
						}
						else {
							if (m_selection.GetChanStart() == PointPos.m_iChannel && PointPos.m_iColumn >= m_selection.GetColStart())
								m_bDragStart = true;
							else if (m_selection.GetChanEnd() == PointPos.m_iChannel && PointPos.m_iColumn <= m_selection.GetColEnd())
								m_bDragStart = true;
							else if (m_selection.GetChanStart() < PointPos.m_iChannel && m_selection.GetChanEnd() > PointPos.m_iChannel)
								m_bDragStart = true;
							else {
								m_bDragStart = false;
								m_bDragging = false;
								m_bSelecting = false;
							}
						}
					}
					else {
						m_bDragStart = false;
						m_bDragging = false;
						m_bSelecting = false;
					}
				}

				if (!m_bDragging && !m_bDragStart) {
					// Begin new selection

					if (m_bControlPressed) {
						PointPos.m_iColumn = 0;
					}

					m_selection.SetStart(PointPos);
					m_selection.SetEnd(PointPos);
				}
				else
					m_cpDragPoint = CCursorPos(PointPos.m_iRow, PointPos.m_iChannel, GetSelectColumn(PointPos.m_iColumn));

				m_ptSelStartPoint = point;
				m_bSelectionInvalid = false;
			}
			else {			
				// Clicked outside the patterns
				m_bDragStart = false;
				m_bDragging = false;
				m_bSelecting = false;
			}
		}
	}
}

void CPatternView::OnMouseRDown(CPoint point)
{
	if (point.y < HEADER_HEIGHT) {
		// Channel headers
	}
	else if (point.y > HEADER_HEIGHT) {
		// Pattern area
		int Row = GetRowAtPoint(point.y);
		int Channel = GetChannelAtPoint(point.x);
		int Column = GetColumnAtPoint(point.x);

		if (Channel < 0 || Channel > (signed)m_pDocument->GetAvailableChannels() - 1)
			return;
		if (Row < 0)
			return;
		if (Column < 0)
			return;

		m_cpCursorPos.m_iRow = Row;
		m_cpCursorPos.m_iChannel = Channel;
		m_cpCursorPos.m_iColumn = Column;
	}
}

void CPatternView::OnMouseUp(CPoint point)
{
	m_iScrolling = SCROLL_NONE;

//	m_iMouseClickChan = -1;

	if (point.y < HEADER_HEIGHT) {
		// Channel headers
		if (m_bDragging) {
			m_bDragging = false;
			m_bDragStart = false;
		}
	}
	else if (point.y > HEADER_HEIGHT) {
		// Pattern area
		CCursorPos PointPos = GetCursorAtPoint(point);

		if (point.x < ROW_COL_WIDTH) {
			if (m_bDragging) {
				m_bDragging = false;
				m_bDragStart = false;
			}
			// Row column, move to clicked row
			if (PointPos.m_iRow < 0 || m_bSelecting)
				return;
			m_cpCursorPos.m_iRow = PointPos.m_iRow;
			return;
		}

		if (m_bDragging) {
			m_bDragging = false;
			m_bDragStart = false;
			if (m_selDrag.m_cpStart.m_iChannel != m_selection.m_cpStart.m_iChannel || m_selDrag.m_cpStart.m_iRow != m_selection.m_cpStart.m_iRow || m_selDrag.m_cpStart.m_iColumn != m_selection.m_cpStart.m_iColumn) {

				CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_DRAG_AND_DROP);

				// Move or copy selected pattern data
				stClipData *pClipData = new stClipData;
				Copy(pClipData);

				// Adjust if out of screen
				if (m_selDrag.m_cpStart.m_iChannel < 0) {
					while (m_selDrag.m_cpStart.m_iChannel < 0) {
						for (int r = 0; r < pClipData->Rows; ++r) {
							for (int c = 1; c < pClipData->Channels; ++c) {
								pClipData->Pattern[c-1][r] = pClipData->Pattern[c][r];
							}
						}
						--pClipData->Channels;
						++m_selDrag.m_cpStart.m_iChannel;
					}
					m_selDrag.m_cpStart.m_iColumn = 0;
				}

				if (m_selDrag.m_cpEnd.m_iChannel > (signed)m_pDocument->GetAvailableChannels() - 1) {
					m_selDrag.m_cpEnd.m_iChannel = (signed)m_pDocument->GetAvailableChannels() - 1;
					m_selDrag.m_cpEnd.m_iColumn = m_pDocument->GetChannelColumns(m_pDocument->GetAvailableChannels());
				}

				if (m_selDrag.m_cpStart.m_iRow < 0) {
					while (m_selDrag.m_cpStart.m_iRow < 0) {
						for (int c = 0; c < pClipData->Channels; ++c) {
							for (int r = 1; r < pClipData->Rows; ++r) {
								pClipData->Pattern[c][r-1] = pClipData->Pattern[c][r];
							}
						}
						--pClipData->Rows;
						++m_selDrag.m_cpStart.m_iRow;
					}
				}

				if (m_selDrag.m_cpEnd.m_iRow > m_iPatternLength - 1)
					m_selDrag.m_cpEnd.m_iRow = m_iPatternLength - 1;

				if (m_selDrag.m_cpEnd.m_iColumn > 15)
					m_selDrag.m_cpEnd.m_iColumn = 15;

				pAction->SetDragAndDrop(pClipData, !m_bControlPressed && !m_bShiftPressed, m_bShiftPressed, &m_selDrag);
            qDebug("DRAG AND DROP!!?");
//				((CMainFrame*) m_pView->GetParentFrame())->AddAction(pAction);

				SAFE_RELEASE(pClipData);
			}
		}
		else if (m_bDragStart && !m_bDragging) {
			m_bDragStart = false;
			m_bSelecting = false;
		}

		if (m_bSelecting)
			return;

		if (PointPos.m_iChannel < 0 || PointPos.m_iChannel > (signed)m_pDocument->GetAvailableChannels() - 1)
			return;
		if (PointPos.m_iRow < 0)
			return;
		if (PointPos.m_iColumn < 0)
			return;

		m_cpCursorPos = PointPos;
	}
}

void CPatternView::OnMouseMove(UINT nFlags, CPoint point)
{
	// Called only when lbutton is active

	 if (!m_bSelectedAll) {

		// Pattern area
		CCursorPos PointPos = GetCursorAtPoint(point);

		if (/*PointPos.m_iChannel != -1 &&*/ PointPos.m_iRow != -1 && PointPos.m_iColumn != -1) {
			if (!m_bSelecting) {
				// Enable selection only if in the pattern field
				if (point.x < (m_iPatternWidth + ROW_COL_WIDTH) && !m_bSelectionInvalid) {
					// Selection threshold
					if (abs(m_ptSelStartPoint.x - point.x) > SELECT_THRESHOLD || abs(m_ptSelStartPoint.y - point.y) > SELECT_THRESHOLD)
						m_bSelecting = true;
				}
			}
			else {
				// Selecting or dragging

				if (PointPos.m_iRow < 0)
					PointPos.m_iRow = 0;
				if (PointPos.m_iRow >= m_iPatternLength)
					PointPos.m_iRow = m_iPatternLength - 1;

				if (PointPos.m_iChannel < 0) {
					PointPos.m_iChannel = 0;
	//				Column = 0;
				}
				if (PointPos.m_iChannel > (signed)m_pDocument->GetAvailableChannels() - 1) {
					PointPos.m_iChannel = (signed)m_pDocument->GetAvailableChannels() - 1;
					PointPos.m_iColumn = m_pDocument->GetEffColumns(PointPos.m_iChannel) * 3 + 4;
				}

				if (m_bDragStart) {
					// Dragging
					if (abs(m_ptSelStartPoint.x - point.x) > SELECT_THRESHOLD || abs(m_ptSelStartPoint.y - point.y) > SELECT_THRESHOLD)
						m_bDragging = true;

					if (m_bDragging) {
						int ChanOffset = PointPos.m_iChannel - m_cpDragPoint.m_iChannel;
						int RowOffset = PointPos.m_iRow - m_cpDragPoint.m_iRow;
						int ColumnStart, ColumnEnd;

						ColumnStart = m_selection.GetColStart();
						ColumnEnd = m_selection.GetColEnd();

						if (IsSingleChannelSelection() && GetSelectColumn(m_selection.GetColStart()) >= COLUMN_EFF1) {
							// Allow dragging between effect columns in the same channel
							if (GetSelectColumn(PointPos.m_iColumn) >= COLUMN_EFF1) {
								ColumnStart = PointPos.m_iColumn - (((PointPos.m_iColumn - 1) % 3));
							}
							else {
								ColumnStart = 4;
							}
							ColumnEnd = ColumnStart + (m_selection.GetColEnd() - m_selection.GetColStart());
						} 

						m_selDrag.m_cpStart = CCursorPos(m_selection.GetRowStart() + RowOffset, m_selection.GetChanStart() + ChanOffset, ColumnStart);
						m_selDrag.m_cpEnd = CCursorPos(m_selection.GetRowEnd() + RowOffset, m_selection.GetChanEnd() + ChanOffset, ColumnEnd);
					}
				}
				else {
					// Selecting 

					if (m_bControlPressed) {
						if (PointPos.m_iChannel >= m_selection.m_cpStart.m_iChannel) {
							PointPos.m_iColumn = m_pDocument->GetEffColumns(PointPos.m_iChannel) * 3 + 4;
							m_selection.m_cpStart.m_iColumn = 0;
						}
						else {
							PointPos.m_iColumn = 0;
							m_selection.m_cpStart.m_iColumn = m_pDocument->GetEffColumns(m_selection.m_cpStart.m_iChannel) * 3 + 4;
						}
					}

					if (m_bFullRowSelect) {
						m_selection.m_cpEnd.m_iRow = PointPos.m_iRow;
					}
					else {
						m_selection.SetEnd(PointPos);
					}
				}
			}
		}

		// Auto-scrolling
		if (m_bSelecting) {
			m_ptScrollMousePos = point;
			m_nScrollFlags = nFlags;

			if ((PointPos.m_iRow - m_iMiddleRow) > (m_iVisibleRows / 2)-3) {
				if (m_cpCursorPos.m_iRow < m_iPatternLength && m_iMiddleRow < (m_iPatternLength - (m_iVisibleRows / 2)+2))
					m_iScrolling = SCROLL_DOWN;
				else
					m_iScrolling = SCROLL_NONE;
			}
			else if ((PointPos.m_iRow - m_iMiddleRow) <= -(m_iVisibleRows / 2)) {
				if (m_cpCursorPos.m_iRow > 0 && m_iMiddleRow > (m_iVisibleRows / 2))
					m_iScrolling = SCROLL_UP;
				else
					m_iScrolling = SCROLL_NONE;
			}
			else if (PointPos.m_iChannel >= (m_iChannelsVisible - 1) && m_iChannelsVisible < m_iChannels) {
				if (m_cpCursorPos.m_iChannel < m_iChannels)
					m_iScrolling = SCROLL_RIGHT;
				else
					m_iScrolling = SCROLL_NONE;
			}
			else if (PointPos.m_iChannel < m_iFirstChannel) {
				if (m_cpCursorPos.m_iChannel > 0)
					m_iScrolling = SCROLL_LEFT;
				else
					m_iScrolling = SCROLL_NONE;
			}
			else
				m_iScrolling = SCROLL_NONE;
		}
	}
}

void CPatternView::OnMouseDblClk(CPoint point)
{
	if (point.y < HEADER_HEIGHT) {
		// Channel headers
		int Channel = GetChannelAtPoint(point.x);
		int Column = GetColumnAtPoint(point.x);

		if (Channel < 0 || Channel > (signed)m_pDocument->GetAvailableChannels() - 1)
			return;

//		m_iMouseClickChan = Channel;
		
		// Solo
		if (Column < 5) {
			m_pView->SoloChannel(Channel);
		}		
		// Remove one track effect column
		else if (Column == 5) {
			int Columns = m_pDocument->GetEffColumns(Channel);
			if (Columns > 0) {
				m_pDocument->SetEffColumns(Channel, Columns - 1);
				m_pDocument->UpdateAllViews(NULL, CHANGED_ERASE);
			}
		}
		// Add one track effect column
		else if (Column == 6) {
			int Columns = m_pDocument->GetEffColumns(Channel);
			if (Columns < (MAX_EFFECT_COLUMNS - 1)) {
				m_pDocument->SetEffColumns(Channel, Columns + 1);
				m_pDocument->UpdateAllViews(NULL, CHANGED_ERASE);
			}
		}
	}
	else if (point.y > HEADER_HEIGHT) {
		// Select whole channel
		SelectAllChannel();
		if (point.x < ROW_COL_WIDTH)
			// Select whole frame
			SelectAll();
		m_bSelectedAll = true;
	}
}