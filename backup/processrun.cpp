/*
 * =====================================================================================
 *
 *       Filename: processrun.cpp
 *        Created: 8/31/2015 3:43:46 AM
 *  Last Modified: 03/17/2016 00:29:52
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

#include <cmath>
#include <mutex>
#include "game.hpp"
#include "processrun.hpp"
#include "clientmessagedef.hpp"

ProcessRun::ProcessRun(Game* pGame)
    : Process(PROCESSID_RUN, pGame)
	, m_MyHero(nullptr)
{
    m_WindowW = GetConfigurationManager()->GetInt("Root/Window/SizeW");
    m_WindowH = GetConfigurationManager()->GetInt("Root/Window/SizeH");
}

ProcessRun::~ProcessRun()
{}

void ProcessRun::Enter()
{
	Process::Enter();

    // delete m_MyHero;
    // m_MyHero = new MyHero(0, 0, 0);
    // m_MyHero->SetPosition(800, 400);
}

void ProcessRun::Exit()
{
	Process::Exit();
}

void ProcessRun::LoadMap(const char *szMapFileName)
{
    std::string szMapFullFileName = 
        ((std::string(GetConfigurationManager()->GetString("Root/Map/Path")) + "/") + szMapFileName) + "/desc.bin";

    m_Map.NewLoadMap(szMapFullFileName.c_str());
}


void ProcessRun::RollScreen()
{
    // what we should have
    int nDX = m_MyHero->X() - m_WindowW / 2 - m_Map.ViewX();
    int nDY = m_MyHero->Y() - m_WindowH / 2 - m_Map.ViewY();

    int nCurrentViewX = m_Map.ViewX();
    int nCurrentViewY = m_Map.ViewY();

    if(std::abs(nDX) > 20){
        nCurrentViewX += std::lround(std::copysign(1.0, nDX));
    }
    if(std::abs(nDY) > 20){
        nCurrentViewY += std::lround(std::copysign(1.0, nDY));
    }

    int nStartX = (std::min)((std::max)(0, nCurrentViewX), m_Map.W() * 48 - m_WindowW);
    int nStartY = (std::min)((std::max)(0, nCurrentViewY), m_Map.H() * 32 - m_WindowH);

    m_Map.SetViewPoint(nStartX, nStartY);
}

void ProcessRun::Draw()
{
    RollScreen();
    // what we want
    int nStartX = m_Map.ViewX();
    int nStartY = m_Map.ViewY();

    int nStartCellX = (std::max)(0, (nStartX / 48) - 4);
    int nStartCellY = (std::max)(0, (nStartY / 32) - 4);
    int nStopCellX  = (std::min)(m_Map.W() - 1, (nStartX / 48) + 44);
    int nStopCellY  = (std::min)(m_Map.H() - 1, (nStartY / 32) + 44);

    m_Map.DrawBaseTile(nStartCellX, nStartCellY, nStopCellX, nStopCellY);
    m_Map.DrawGroundObject(nStartCellX, nStartCellY, nStopCellX, nStopCellY);

    {
        std::lock_guard<std::mutex> stGuard(m_ActorListMutex);
		m_ActorList.sort([](Actor *pA1, Actor *pA2){return pA1->Y() < pA2->Y(); });
    }

    auto fnExtDraw = [this](int nX, int nY){
        { // draw my hero
            std::lock_guard<std::mutex> stGuard(m_MyHeroMutex);
            if(m_MyHero->X() / 48 == nX && m_MyHero->Y() / 32 == nY){
                m_MyHero->Draw();
            }
        }
        { // draw all actors
            std::lock_guard<std::mutex> stGuard(m_ActorListMutex);
            for(auto pActor: m_ActorList){
                if(pActor->X() / 48 == nX && pActor->Y() / 32 == nY){
                    pActor->Draw();
                }
            }
        }
    };
    m_Map.DrawOverGroundObject(nStartCellX, nStartCellY, nStopCellX, nStopCellY, fnExtDraw);
}

void ProcessRun::UpdateActor()
{
    std::lock_guard<std::mutex> stGuard(m_ActorListMutex);
    for(auto pActor: m_ActorList){
		pActor->Update();
    }
}

void ProcessRun::Update()
{
    // TODO
    {
        std::lock_guard<std::mutex> stGuard(m_ActorListMutex);
        for(auto pActor: m_ActorList){
            if(pActor != m_MyHero){
                pActor->Update();
            }
        }
        {
            std::lock_guard<std::mutex> stGuard(m_MyHeroMutex);
            m_MyHero->EstimateNextPosition();
            int nNextX = m_MyHero->EstimateNextX();
            int nNextY = m_MyHero->EstimateNextY();

            for(auto pActor: m_ActorList){
                if(pActor != m_MyHero){
                    pActor->Update();
                }
            }
        }

    }
	UpdateMyHero();
    UpdateActor();

	auto fnMessageHandler = [this](const Message &stMessage){
		HandleMessage(stMessage);
	};

	GetMessageManager()->BatchHandleMessage(fnMessageHandler);
}

void ProcessRun::UpdateMyHero()
{
	std::lock_guard<std::mutex> stGuard(m_MyHeroMutex);
	m_MyHero->Update();
}

void ProcessRun::HandleMessage(const Message &stMessage)
{
    switch(stMessage.Index()){
        case CLIENTMT_ACTORBASEINFO:
            {
                ClientMessageActorBaseInfo stTmpCM;
                std::memcpy(&stTmpCM, stMessage.Body(), sizeof(stTmpCM));
                std::lock_guard<std::mutex> stGuard(m_ActorListMutex);
				bool bFind = false;
                for(auto pActor: m_ActorList){
                    if(true
                            && pActor->UID()     == stTmpCM.nUID
                            && pActor->SID()     == stTmpCM.nSID
                            && pActor->GenTime() == stTmpCM.nGenTime
                      ){
						bFind = true;
                        pActor->SetNextState(stTmpCM.nState);
                        pActor->SetNextPosition(stTmpCM.nX, stTmpCM.nY);
                        // pActor->SetHP(stTmpCM.nHP);
                    }
                }

				if(!bFind){
					// can't find it, create a new one
					auto pActor = NewActor(stTmpCM.nSID, stTmpCM.nUID, stTmpCM.nGenTime);
					pActor->SetDirection(stTmpCM.nDirection);
					pActor->SetNextState(stTmpCM.nState);
					// pActor->SetNextPosition(stTmpCM.nX, stTmpCM.nY);
					pActor->SetMap(stTmpCM.nX, stTmpCM.nY, &m_Map);
					m_ActorList.push_back(pActor);
					// pActor->SetHP(stTmpCM.nHP);
				}
                break;
            }
        default:
            break;
    }
}

Actor *ProcessRun::NewActor(int nSID, int nUID, int nGenTime)
{
    switch(nSID){
        case 1000:
        case 1001:
        case 1002:
        case 1003:
        case 1004:
        case 1005:
            {
                return new Hero(nSID, nUID, nGenTime);
            }
        default:
            {
                return new Monster(nSID, nUID, nGenTime);
            }
    }
}

void ProcessRun::HandleEvent(SDL_Event *pEvent)
{
    switch(pEvent->type){
        case SDL_MOUSEBUTTONDOWN:
            {
                switch(pEvent->button.button){
                    case SDL_BUTTON_RIGHT:
                        {
                            int nDX = pEvent->button.x - m_MyHero->ScreenX();
                            int nDY = pEvent->button.y - m_MyHero->ScreenY();
                            m_MyHero->DGoto(nDX, nDY);
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case SDL_MOUSEMOTION:
            {
                break;
            }
        case SDL_KEYDOWN:
            {
                m_MyHero->SetState(0);
                break;
            }
        default:
            {
                break;
            }
    }
}

void ProcessRun::OnLoginSucceed(const ClientMessageLoginSucceed & stCMLS)
{
    LoadMap(stCMLS.szMapName);
    delete m_MyHero;
    m_MyHero = new MyHero(stCMLS.nSID, stCMLS.nUID, stCMLS.nGenTime);
    m_MyHero->SetMap(stCMLS.nMapX, stCMLS.nMapY, &m_Map);
    m_MyHero->SetDirection(stCMLS.nDirection);

    int nStartX = (std::min)((std::max)(0, m_MyHero->X() - m_WindowW / 2), m_Map.W() * 48 - m_WindowW);
    int nStartY = (std::min)((std::max)(0, m_MyHero->Y() - m_WindowH / 2), m_Map.H() * 32 - m_WindowH);

    m_Map.SetViewPoint(nStartX, nStartY);
}
