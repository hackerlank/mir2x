/*
 * =====================================================================================
 *
 *       Filename: inputboard.cpp
 *        Created: 08/21/2015 7:04:16 PM
 *  Last Modified: 03/20/2016 23:52:15
 *
 *    Description: 
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include <SDL2/SDL.h>
#include <utf8.h>
#include <algorithm>
#include "inputboard.hpp"
#include "sdlkeyeventchar.hpp"

int InputBoard::m_ShowSystemCursorCount = 0;
int InputBoard::m_InputBoxCount         = 0;

InputBoard::InputBoard(bool bWrap, int nW, int nH,
        int nMinTextMargin, int nMinTextLineSpace,
        uint8_t nFontSet, uint8_t nFontSize, uint32_t nTextColor,
        int nCursorWidth, uint32_t nCursorColor)
    : Widget()
    , m_CursorWidth(nCursorWidth)
    , m_CursorColor(nCursorColor)
    , m_FontSet(nFontSet)
    , m_Size(nFontSize)
    , m_TextColor(nTextColor)
    , m_SystemCursorX(0)
    , m_SystemCursorY(0)
    , m_DrawOwnSystemCursor(false)
    , m_ShowStartX(0)
    , m_Ticks(0)
    , m_Focus(false)
    , m_Content("")
    , m_TokenBoard(bWrap, nW, nMinTextMargin, nMinTextLineSpace)
    , m_IME(nullptr)
{
    m_InputBoxCount++;
    m_ShowSystemCursorCount++;
    m_W = nW;
    m_H = nH;

    SetProperH();
}

InputBoard::~InputBoard()
{
    m_InputBoxCount--;
    m_ShowSystemCursorCount--;
}

void InputBoard::Update(Uint32 nMs)
{
    m_Ticks += nMs;
}

bool InputBoard::ProcessEvent(const SDL_Event &rstEvent)
{
    switch(rstEvent.type){
        case SDL_MOUSEMOTION:
            {
                if(In(rstEvent.motion.x, rstEvent.motion.y)){
                    m_SystemCursorX    = rstEvent.motion.x;
                    m_SystemCursorY    = rstEvent.motion.y;
                    if(!m_DrawOwnSystemCursor){
                        m_ShowSystemCursorCount--;
                    }
                    m_DrawOwnSystemCursor = true;
                }else{
                    if(m_DrawOwnSystemCursor){
                        m_ShowSystemCursorCount++;
                    }
                    m_DrawOwnSystemCursor = false;
                }
                break;
            }
        case SDL_MOUSEBUTTONDOWN:
            {
                if(In(rstEvent.button.x, rstEvent.button.y)){
                    m_Focus = true;
                    BindCursorTokenBox(rstEvent.button.x, rstEvent.button.y);
                    ResetShowStartX();
                    return true;
                }else{
                    m_Focus = false;
                }
                break;
            }
        case SDL_KEYDOWN:
            {
                if(m_Focus){
                    // clear the count
                    m_Ticks = 0;


                    if(m_IME){
                        return m_IME->ProcessEvent(rstEvent);
                    }

                    char chKeyName = SDLKeyEventChar(rstEvent);


                    if(chKeyName != '\0'){
                        m_BindTokenBoxIndex++;
                        m_Content.insert((std::size_t)(m_BindTokenBoxIndex), (std::size_t)1, chKeyName);
                        Compile();
                        ResetShowStartX();
                        return true;
                    }

                    if(rstEvent.key.keysym.sym == SDLK_BACKSPACE){
                        if(m_BindTokenBoxIndex >= 0){
                            m_Content.erase(m_BindTokenBoxIndex, 1);
                            m_BindTokenBoxIndex--;
                            Compile();
                            ResetShowStartX();
                        }
                        return true;
                    }

                    if(rstEvent.key.keysym.sym == SDLK_LEFT){
                        if(m_BindTokenBoxIndex >= 0){
                            m_BindTokenBoxIndex--;
                            ResetShowStartX();
                        }
                        return true;
                    }

                    if(rstEvent.key.keysym.sym == SDLK_RIGHT){
                        if((size_t)(m_BindTokenBoxIndex + 1) < m_Content.size()){
                            m_BindTokenBoxIndex++;
                            ResetShowStartX();
                        }
                        return true;
                    }
                }
                break;
            }
        default:
            break;
    }
    return false;
}


void InputBoard::BindCursorTokenBox(int nEventX, int nEventY)
{
    // +-------------------------- <------- screen corner
    // |
    // |
    // |   +----------------------+ <------ full tokenbox
    // |   |                      |
    // |   |  +--------------+    | <------ the inputboard window
    // |   |  |              |    |
    // |   |  |              |    |
    // |   |  |  x           |    | <------ event point
    // |   |  |              |    |
    // |   |  +--------------+    |
    // |   |                      |
    // |   |                      |
    // |   +----------------------+

    int nRow, nCol;

    if(m_TokenBoard.TokenBoxUnderPoint(nEventX, nEventY, nRow, nCol)){
        m_TokenBoard.TokenBoxLocationInfo(nRow, nCol,
                nStartX, nStartY, nW, nW1, nW2, nH, nH1, nH2);

        m_BindTokenBoxLocation = {nRow, nCol};


    }

    int nX = nEventX - X() + m_ShowStartX;
    int nY = nEventY - Y() + m_ShowStartY;

    for(int nIndex = 0; nIndex < (int)m_Line.size(); ++nIndex){
        int nBoxStartX = m_Line[nIndex].Cache.StartX - m_Line[nIndex].State.W1;
        int nBoxStartY = m_Line[nIndex].Cache.StartY;
        int nBoxW      = m_Line[nIndex].State.W1 + m_Line[nIndex].Cache.W + m_Line[nIndex].State.W2;
        int nBoxH      = m_Line[nIndex].Cache.H;
        if(PointInRect(nX, nY, nBoxStartX, nBoxStartY, nBoxW, nBoxH)){
            if(PointInRect(nX, nY, nBoxStartX, nBoxStartY, nBoxW / 2, nBoxH)){
                m_BindTokenBoxIndex = nIndex - 1;
            }else{
                m_BindTokenBoxIndex = nIndex;
            }
            return;
        }
    }
    m_BindTokenBoxIndex = m_Line.size() - 1;
}


void InputBoard::ResetShowStartX()
{
}

void InputBoard::ResetShowStartX()
{
    if(m_BindTokenBoxIndex < 0){
        m_ShowStartX = 0;
        return;
    }

    if(m_Line[m_BindTokenBoxIndex].Cache.StartX < m_ShowStartX){
        m_ShowStartX = m_Line[m_BindTokenBoxIndex].Cache.StartX
            - m_Line[m_BindTokenBoxIndex].State.W1;
        return;
    }

    int nBoxEndX = m_Line[m_BindTokenBoxIndex].Cache.StartX
        + m_Line[m_BindTokenBoxIndex].State.W1
        + m_Line[m_BindTokenBoxIndex].State.W2
        + m_Line[m_BindTokenBoxIndex].Cache.W - 1;

    if(nBoxEndX > m_ShowStartX + m_W - 1){
        m_ShowStartX = nBoxEndX - m_W + 1;
        return;
    }

    // otherwise m_BindTokenBoxIndex is ok
    // do nothing ??
}


void InputBoard::Draw()
{
    if(m_TokenBoard.W() + m_CursorWidth <= m_W){
        m_TokenBoard.Draw(X(), Y());
    }else{
        m_TokenBoard.Draw(X(), Y(), 
                m_W - m_TokenBoard.W() - m_CursorWidth, 0);
    }
}

void InputBoard::Draw()
{
    int  nX = X();
    int  nY = Y();
    bool bStartDraw = false;

    for(int nIndex = 0; nIndex < m_Line.size(); ++nIndex){
        int nBoxStartX = m_Line[nIndex].Cache.StartX;
        int nBoxStopX  = m_Line[nIndex].Cache.StartX + m_Line[nIndex].Cache.W - 1;

        if(nBoxStopX >= m_ShowStartX || nBoxStartX <= (m_ShowStartX + m_W - 1)){
            int nCutX1         = (std::max)(0, m_ShowStartX - nBoxStartX);
            int nCutX2         = (std::max)(0, nBoxStopX - (m_ShowStartX + m_W - 1));
            int nDrawBoxStartX = nCutX1;
            int nDrawBoxStopX  = (m_Line[nIndex].Cache.W - 1) - nCutX2;

            SDL_Rect stSrc, stDst;

            stSrc.x = nDrawBoxStartX;
            stSrc.y = 0;
            stSrc.w = nDrawBoxStopX - nDrawBoxStartX + 1;
            stSrc.h = m_Line[nIndex].Cache.H;

            stDst.x = nX + m_Line[nIndex].Cache.StartX + stSrc.x - m_ShowStartX;
            stDst.y = nY + (m_H - m_Line[nIndex].Cache.H);
            stDst.w = stSrc.w;
            stDst.h = stSrc.h;

            SDL_RenderCopy(GetDeviceManager()->GetRenderer(),
                    m_Line[nIndex].UTF8CharBox.Cache.Texture[0],
                    &stSrc, &stDst);
            bStartDraw = true;
        }else{
            if(bStartDraw){
                return;
            }
        }
    }
    DrawCursor();
    DrawSystemCursor();
}

void InputBoard::DeleteBindTokenBox()
{
    if(m_BindTokenBoxIndex.second < 0){
        if(m_BindTokenBoxIndex.first > 0){
            // at the beginning of some line

            Compile();
        }
    }else{
        // bind to some tokenbox for sure

        Compile();
    }
}

void InputBoard::DrawCursor()
{
    if(m_Ticks % 1000 < 500 && m_Focus){
        if(m_BindTokenBoxIndex.first >= 0 && m_BindTokenBoxIndex.second >= 0){
            // cursor binding to some box
            int nStartX, nStartY, nW, nW1, nW2, nH, nH1, nH2;
            m_TokenBoard.TokenBoxLocationInfo(m_BindTokenBoxIndex.first, m_BindTokenBoxIndex.second,
                    nStartX, nStartY, nW, nW1, nW2, nH, nH1, nH2);

            fnDrawCursor(X() + nStartX + nW - nW1 - nW2,
                    Y() + nStartY + nH - nH1 - nH2, m_CursorWidth, m_CursorColor);

        }else{
            if(m_BindTokenBoxIndex.second < 0){
                // start with empty line
            }
        }
    }
}

void InputBoard::DrawSystemCursor()
{
    if(m_DrawOwnSystemCursor){
        GetDeviceManager()->SetRenderDrawColor(200, 200, 200, 200);
        SDL_RenderDrawLine(
                GetDeviceManager()->GetRenderer(),
                m_SystemCursorX, m_SystemCursorY - m_H / 2,
                m_SystemCursorX, m_SystemCursorY + m_H / 2);
        SDL_RenderDrawLine(
                GetDeviceManager()->GetRenderer(),
                m_SystemCursorX - (std::max)(1, (int)std::lround(m_H * 0.08)),
                m_SystemCursorY - m_H / 2,
                m_SystemCursorX + (std::max)(1, (int)std::lround(m_H * 0.08)),
                m_SystemCursorY - m_H / 2);
        SDL_RenderDrawLine(
                GetDeviceManager()->GetRenderer(),
                m_SystemCursorX - (std::max)(1, (int)std::lround(m_H * 0.08)),
                m_SystemCursorY + m_H / 2,
                m_SystemCursorX + (std::max)(1, (int)std::lround(m_H * 0.08)),
                m_SystemCursorY + m_H / 2);
        GetDeviceManager()->SetRenderDrawColor(0, 0, 0, 0);
    }

    if(m_ShowSystemCursorCount < m_InputBoxCount){
        SDL_ShowCursor(0);
    }else{
        SDL_ShowCursor(1);
    }
}

void InputBoard::Compile()
{
    m_XMLContent.clear();

    m_Line.clear();

    TOKENBOX stTokenBox;

    const char *pStart = m_Content.c_str();
    const char *pEnd   = pStart;

    while(*pEnd != '\0'){
        pStart = pEnd;
        utf8::unchecked::advance(pEnd, 1);
        std::memset(stTokenBox.UTF8CharBox.Data, 0, 8);
        if(pEnd - pStart == 1 && (*pStart == '\n' || *pStart == '\t' || *pStart == '\r')){
            // continue;
            stTokenBox.UTF8CharBox.Data[0] = ' ';
        }else{
            std::memcpy(stTokenBox.UTF8CharBox.Data, pStart, pEnd - pStart);
        }
        LoadUTF8CharBoxCache(stTokenBox);
        PushBack(stTokenBox);
    }
    SetTokenBoxStartX();
}

void InputBoard::SetTokenBoxStartX()
{
    int nCurrentX = 0;
    for(auto &stTokenBox: m_Line){
        nCurrentX += stTokenBox.State.W1;
        stTokenBox.Cache.StartX = nCurrentX;
        nCurrentX += stTokenBox.Cache.W;
        nCurrentX += stTokenBox.State.W2;
    }
}

void InputBoard::PushBack(TOKENBOX &stTokenBox)
{
    stTokenBox.State.W1 = 0;
    // stTokenBox.State.W2 = 0;
    stTokenBox.State.W2 = 1;
    m_Line.push_back(stTokenBox);
}

void InputBoard::LoadUTF8CharBoxCache(TOKENBOX &stTokenBox)
{
    std::memcpy(m_CharBoxCache.Data, stTokenBox.UTF8CharBox.Data, 8);

    stTokenBox.UTF8CharBox.Cache.Texture[0] = GetFontTextureManager()->RetrieveTexture(m_CharBoxCache);
    SDL_QueryTexture(stTokenBox.UTF8CharBox.Cache.Texture[0],
            nullptr, nullptr, &(stTokenBox.Cache.W), &(stTokenBox.Cache.H));
    stTokenBox.Cache.H1     = stTokenBox.Cache.H;
    stTokenBox.Cache.H2     = 0;
    stTokenBox.Cache.StartY = 0;

    m_H = (std::max)(m_H, stTokenBox.Cache.H);
}

void InputBoard::SetContent(const char *szInfo)
{
    if(szInfo){
        m_Content = szInfo;
    }else{
        m_Content = "";
    }
    Compile();
    m_BindTokenBoxIndex = m_Content.size() - 1;
    ResetShowStartX();
}

const char *InputBoard::Content()
{
    return m_Content.c_str();
}

void InputBoard::SetProperH()
{
    TOKENBOX stTokenBox;
    stTokenBox.UTF8CharBox.Data[0] = ' ';
    stTokenBox.UTF8CharBox.Data[1] = '\0';
    LoadUTF8CharBoxCache(stTokenBox);
}
