/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "system.h"
#include "GUIWindowVideoFiles.h"
#include "Util.h"
#include "Picture.h"
#include "utils/IMDB.h"
#include "utils/GUIInfoManager.h"
#include "PlayListFactory.h"
#include "Application.h"
#include "NfoFile.h"
#include "PlayListPlayer.h"
#include "GUIPassword.h"
#include "GUIDialogMediaSource.h"
#include "GUIDialogContentSettings.h"
#include "GUIDialogVideoScan.h"
#include "FileSystem/MultiPathDirectory.h"
#include "utils/RegExp.h"
#include "GUIWindowManager.h"
#include "GUIDialogOK.h"
#include "GUIDialogYesNo.h"
#include "FileSystem/File.h"
#include "PlayList.h"
#include "Settings.h"
#include "GUISettings.h"
#include "LocalizeStrings.h"
#include "utils/log.h"
#include "BackgroundMusicPlayer.h"

using namespace std;
using namespace XFILE;
using namespace PLAYLIST;
using namespace VIDEO;

#define CONTROL_PLAY_DVD           6
#define CONTROL_STACK              7
#define CONTROL_BTNSCAN            8
#define CONTROL_BTNPLAYLISTS      13

CGUIWindowVideoFiles::CGUIWindowVideoFiles()
: CGUIWindowVideoBase(WINDOW_VIDEO_FILES, "MyVideo.xml")
{
  m_stackingAvailable = true;
  m_cleaningAvailable = true;
}

CGUIWindowVideoFiles::~CGUIWindowVideoFiles()
{
}

bool CGUIWindowVideoFiles::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_WINDOW_INIT:
    {
      // is this the first time accessing this window?
      if (m_vecItems->m_strPath == "?" && message.GetStringParam().IsEmpty())
        message.SetStringParam(g_settings.m_defaultVideoSource);

      return CGUIWindowVideoBase::OnMessage(message);
    }
    break;

  case GUI_MSG_CLICKED:
    {
      int iControl = message.GetSenderId();
      //if (iControl == CONTROL_BTNSCAN)
      //{
      //  OnScan();
     // }
      /*else*/ if (iControl == CONTROL_STACK)
      {
        // toggle between the following states:
        //   0 : no stacking
        //   1 : stacking
        g_settings.m_iMyVideoStack++;

        if (g_settings.m_iMyVideoStack > STACK_SIMPLE)
          g_settings.m_iMyVideoStack = STACK_NONE;

        g_settings.Save();
        UpdateButtons();
        Update( m_vecItems->m_strPath );
      }
      else if (iControl == CONTROL_BTNPLAYLISTS)
      {
        if (!m_vecItems->m_strPath.Equals("special://videoplaylists/"))
        {
          CStdString strParent = m_vecItems->m_strPath;
          UpdateButtons();
          Update("special://videoplaylists/");
        }
      }
      // list/thumb panel
      else if (m_viewControl.HasControl(iControl))
      {
        int iItem = m_viewControl.GetSelectedItem();
        int iAction = message.GetParam1();

        const CFileItemPtr pItem = m_vecItems->Get(iItem);

        // use play button to add folders of items to temp playlist
        if (iAction == ACTION_PLAYER_PLAY && pItem->m_bIsFolder && !pItem->IsParentFolder())
        {
#ifdef HAS_DVD_DRIVE
          if (pItem->IsDVD())
            return MEDIA_DETECT::CAutorun::PlayDisc();
#endif

          if (pItem->m_bIsShareOrDrive)
            return false;
          // if playback is paused or playback speed != 1, return
          if (g_application.IsPlayingVideo())
          {
            if (g_application.m_pPlayer->IsPaused()) return false;
            if (g_application.GetPlaySpeed() != 1) return false;
          }
          // not playing, or playback speed == 1
          // queue folder or playlist into temp playlist and play
          if ((pItem->m_bIsFolder && !pItem->IsParentFolder()) || pItem->IsPlayList())
            PlayItem(iItem);
          return true;
        }
      }
    }
  }
  return CGUIWindowVideoBase::OnMessage(message);
}

bool CGUIWindowVideoFiles::OnAction(const CAction &action)
{
  if (action.GetID() == ACTION_TOGGLE_WATCHED)
  {
    CFileItemPtr pItem = m_vecItems->Get(m_viewControl.GetSelectedItem());
    if (pItem->IsParentFolder())
      return false;

    if (pItem && pItem->GetOverlayImage().Equals("OverlayWatched.png"))
      return OnContextButton(m_viewControl.GetSelectedItem(),CONTEXT_BUTTON_MARK_UNWATCHED);
    else
      return OnContextButton(m_viewControl.GetSelectedItem(),CONTEXT_BUTTON_MARK_WATCHED);
  }
  return CGUIWindowVideoBase::OnAction(action);
}

void CGUIWindowVideoFiles::UpdateButtons()
{
  CGUIWindowVideoBase::UpdateButtons();
  const CGUIControl *stack = GetControl(CONTROL_STACK);
  if (stack)
  {
    if (m_stackingAvailable)
    {
      CONTROL_ENABLE(CONTROL_STACK);
      if (stack->GetControlType() == CGUIControl::GUICONTROL_RADIO)
      {
        SET_CONTROL_SELECTED(GetID(), CONTROL_STACK, g_settings.m_iMyVideoStack == STACK_SIMPLE);
        SET_CONTROL_LABEL(CONTROL_STACK, 14000);  // Stack
      }
      else
      {
        SET_CONTROL_LABEL(CONTROL_STACK, g_settings.m_iMyVideoStack + 14000);
      }
    }
    else
    {
      if (stack->GetControlType() == CGUIControl::GUICONTROL_RADIO)
      {
        SET_CONTROL_LABEL(CONTROL_STACK, 14000);  // Stack
      }

      CONTROL_DISABLE(CONTROL_STACK);
    }
  }
}

bool CGUIWindowVideoFiles::GetDirectory(const CStdString &strDirectory, CFileItemList &items)
{
  if (!CGUIWindowVideoBase::GetDirectory(strDirectory, items))
    return false;

  ADDON::ScraperPtr info2 = m_database.GetScraperForPath(strDirectory);

  m_stackingAvailable = true;
  m_cleaningAvailable = true;


  if ((info2 && info2->Content() == CONTENT_TVSHOWS) || items.IsTuxBox() || items.IsPlugin() || items.IsAddonsPath() || items.IsRSS() || items.IsInternetStream())
  { // dont stack or clean strings in tv dirs
    m_stackingAvailable = false;
    m_cleaningAvailable = false;
  }
  else if (!items.IsStack() && g_settings.m_iMyVideoStack != STACK_NONE)
    items.Stack();

  //if (info2 && info2->Content() != CONTENT_NONE)
  if (items.GetContent().IsEmpty())
    items.SetContent("files");

  items.SetThumbnailImage("");
  items.SetVideoThumb();
  
  // Send theme update message
  BackgroundMusicPlayer::SendThemeChangeMessage(items.GetProperty("theme"));

  return true;
}

void CGUIWindowVideoFiles::OnPrepareFileItems(CFileItemList &items)
{
  CGUIWindowVideoBase::OnPrepareFileItems(items);
  if (g_guiSettings.GetBool("myvideos.cleanstrings") && !items.IsVirtualDirectoryRoot())
  {
    for (int i = 0; i < (int)items.Size(); ++i)
    {
      CFileItemPtr item = items[i];
      if ((item->m_bIsFolder && !CUtil::IsInArchive(item->m_strPath)) || m_cleaningAvailable)
        item->CleanString();
    }
  }
}

bool CGUIWindowVideoFiles::OnPlayMedia(int iItem)
{
  if ( iItem < 0 || iItem >= (int)m_vecItems->Size() ) return false;
  CFileItemPtr pItem = m_vecItems->Get(iItem);

#ifdef HAS_DVD_DRIVE
  if (pItem->IsDVD())
    return MEDIA_DETECT::CAutorun::PlayDisc();
#endif

  if (pItem->m_bIsShareOrDrive)
    return false;

  AddFileToDatabase(pItem.get());

  return CGUIWindowVideoBase::OnPlayMedia(iItem);
}

void CGUIWindowVideoFiles::AddFileToDatabase(const CFileItem* pItem)
{
  if (!pItem->IsVideo()) return ;
  if ( pItem->IsNFO()) return ;
  if ( pItem->IsPlayList()) return ;

  /* subtitles are assumed not to exist here, if we need it  */
  /* player should update this when it figures out if it has */
  m_database.AddFile(pItem->m_strPath);

  if (pItem->IsStack())
  { // get stacked items
    // TODO: This should be removed as soon as we no longer need the individual
    // files for saving settings etc.
    vector<CStdString> movies;
    GetStackedFiles(pItem->m_strPath, movies);
    for (unsigned int i = 0; i < movies.size(); i++)
    {
      CFileItem item(movies[i], false);
      m_database.AddFile(item.m_strPath);
    }
  }
}

bool CGUIWindowVideoFiles::OnUnAssignContent(const CStdString &path, int label1, int label2, int label3)
{
  bool bCanceled;
  CVideoDatabase db;
  db.Open();
  if (CGUIDialogYesNo::ShowAndGetInput(label1,label2,label3,20022,bCanceled))
  {
    db.RemoveContentForPath(path);
    db.Close();
    CUtil::DeleteVideoDatabaseDirectoryCache();
    return true;
  }
  else
  {
    if (!bCanceled)
    {
      ADDON::ScraperPtr info;
      SScanSettings settings;
      settings.exclude = true;
      db.SetScraperForPath(path,info,settings);
    }
  }
  db.Close();

  return false;
}

void CGUIWindowVideoFiles::OnAssignContent(const CStdString &path, int iFound, ADDON::ScraperPtr& info, SScanSettings& settings)
{
  bool bScan=false;
  CVideoDatabase db;
  db.Open();
  if (iFound == 0)
  {
    info = db.GetScraperForPath(path, settings);
  }

  ADDON::ScraperPtr info2(info);

  if (CGUIDialogContentSettings::Show(info, settings, bScan))
  {
    if(settings.exclude || (!info && info2))
    {
      OnUnAssignContent(path,20375,20340,20341);
    }
    else if (info != info2)
    {
      if (OnUnAssignContent(path,20442,20443,20444))
        bScan = true;
    }

    db.SetScraperForPath(path,info,settings);

    if (bScan)
    {
      CGUIDialogVideoScan* pDialog = (CGUIDialogVideoScan*)g_windowManager.GetWindow(WINDOW_DIALOG_VIDEO_SCAN);
      if (pDialog)
        pDialog->StartScanning(path, true);
    }
  }
}

void CGUIWindowVideoFiles::GetStackedDirectory(const CStdString &strPath, CFileItemList &items)
{
  items.Clear();
  m_rootDir.GetDirectory(strPath, items);
  items.Stack();
}

void CGUIWindowVideoFiles::LoadPlayList(const CStdString& strPlayList)
{
  // load a playlist like .m3u, .pls
  // first get correct factory to load playlist
  auto_ptr<CPlayList> pPlayList (CPlayListFactory::Create(strPlayList));
  if ( NULL != pPlayList.get())
  {
    // load it
    if (!pPlayList->Load(strPlayList))
    {
      CGUIDialogOK::ShowAndGetInput(6, 0, 477, 0);
      return ; //hmmm unable to load playlist?
    }
  }

  int iSize = pPlayList->size();
  if (g_application.ProcessAndStartPlaylist(strPlayList, *pPlayList, PLAYLIST_VIDEO))
  {
    if (m_guiState.get())
      m_guiState->SetPlaylistDirectory("playlistvideo://");
    // activate the playlist window if its not activated yet
    if (GetID() == g_windowManager.GetActiveWindow() && iSize > 1)
    {
      g_windowManager.ActivateWindow(WINDOW_VIDEO_PLAYLIST);
    }
  }
}

void CGUIWindowVideoFiles::GetContextButtons(int itemNumber, CContextButtons &buttons)
{
  CFileItemPtr item;
  if (itemNumber >= 0 && itemNumber < m_vecItems->Size())
    item = m_vecItems->Get(itemNumber);
  
  bool includeStandardContextButtons = true;
  if (item)
  {
    includeStandardContextButtons = item->m_includeStandardContextItems;
    if (m_vecItems->IsVirtualDirectoryRoot())
    {
      // get the usual shares, and anything for all media windows
      CGUIDialogContextMenu::GetContextButtons("video", item, buttons);
      CGUIMediaWindow::GetContextButtons(itemNumber, buttons);
    }
    
    // The info button.
    if (m_vecItems->GetContent() == "movies")
      buttons.Add(CONTEXT_BUTTON_INFO, 13346);
    else if (m_vecItems->GetContent() == "tvshows")
      buttons.Add(CONTEXT_BUTTON_INFO, 20351);
    else if (m_vecItems->GetContent() == "episodes")
      buttons.Add(CONTEXT_BUTTON_INFO, 20352);
    
    if ((item->IsPlexMediaServerLibrary() && m_vecItems->GetContent() != "files") ||
        item->HasProperty("ratingKey"))
    {
      CStdString viewOffset = item->GetProperty("viewOffset");
      
      if (item->GetVideoInfoTag()->m_playCount > 0 || viewOffset.size() > 0)
        buttons.Add(CONTEXT_BUTTON_MARK_UNWATCHED, 16104);
      if (item->GetVideoInfoTag()->m_playCount == 0 || viewOffset.size() > 0)
        buttons.Add(CONTEXT_BUTTON_MARK_WATCHED, 16103);
    }
    
    if (m_vecItems->IsVirtualDirectoryRoot() == false)
      CGUIWindowVideoBase::GetContextButtons(itemNumber, buttons);
  }

  if (includeStandardContextButtons)
    CGUIWindowVideoBase::GetNonContextButtons(itemNumber, buttons);
}

bool CGUIWindowVideoFiles::OnContextButton(int itemNumber, CONTEXT_BUTTON button)
{
  CFileItemPtr item;
  if (itemNumber >= 0 && itemNumber < m_vecItems->Size())
    item = m_vecItems->Get(itemNumber);
  if ( m_vecItems->IsVirtualDirectoryRoot() && item)
  {
    if (CGUIDialogContextMenu::OnContextButton("video", item, button))
    {
      //TODO should we search DB for entries from plugins?
      if (button == CONTEXT_BUTTON_REMOVE_SOURCE && !item->IsPlugin()
          && !item->IsLiveTV() &&!item->IsRSS())
      {
        OnUnAssignContent(item->m_strPath,20375,20340,20341);
      }
      Update("");
      return true;
    }
  }

  switch (button)
  {    
  case CONTEXT_BUTTON_MARK_WATCHED:
    // If we're about to hide this item, select the next one
    //m_viewControl.SetSelectedItem((itemNumber+1) % m_vecItems->Size());
    MarkWatched(item);
    return true;

  case CONTEXT_BUTTON_MARK_UNWATCHED:
    MarkUnWatched(item);
    return true;

  default:
    break;
  }
  return CGUIWindowVideoBase::OnContextButton(itemNumber, button);
}

void CGUIWindowVideoFiles::OnQueueItem(int iItem)
{
  CGUIWindowVideoBase::OnQueueItem(iItem);
}

CStdString CGUIWindowVideoFiles::GetStartFolder(const CStdString &dir)
{
  SetupShares();
  VECSOURCES shares;
  m_rootDir.GetSources(shares);
  bool bIsSourceName = false;
  int iIndex = CUtil::GetMatchingSource(dir, shares, bIsSourceName);
  if (iIndex > -1)
  {
    if (iIndex < (int)shares.size() && shares[iIndex].m_iHasLock == 2)
    {
      CFileItem item(shares[iIndex]);
      if (!g_passwordManager.IsItemUnlocked(&item,"video"))
        return "";
    }
    // set current directory to matching share
    if (bIsSourceName)
      return shares[iIndex].strPath;
    return dir;
  }
  return CGUIWindowVideoBase::GetStartFolder(dir);
}
