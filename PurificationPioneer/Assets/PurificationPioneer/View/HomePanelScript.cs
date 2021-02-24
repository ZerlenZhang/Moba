﻿using PurificationPioneer.Global;
using PurificationPioneer.Network.Const;
using PurificationPioneer.Network.Proxy;
using PurificationPioneer.Script;
using PurificationPioneer.Utility;
using UnityEngine;
using UnityEngine.UI;

namespace PurificationPioneer.View
{
    public class HomePanelScript:MonoBehaviour
    {
        public UserInfoRect userInfoRect;
        public MatchUi matchUi;
        public ItemBar coinBarInfo;
        public ItemBar diamondBarInfo;
        public Button playBtn;
        public Button settingBtn;
        public Button friendBtn;
        public Button mailBtn;
        public Button noticeBtn;
        public Button chatBtn;
        public Button shopBtn;
        public GameObject roomTypeUi;

        public void ShowRoomTypeUi()
        {
            if (!roomTypeUi.activeSelf)
                roomTypeUi.SetActive(true);
        }

        public void HideRoomTypeUi()
        {
            if (roomTypeUi.activeSelf)
                roomTypeUi.SetActive(false);
        }

        public void TryMatchSingleMode()
        {
            HideRoomTypeUi();
            LogicProxy.Instance.StartMatchSingle(GlobalVar.Uname);
        }

        public void TryMatchMultiMode()
        {
            HideRoomTypeUi();
            LogicProxy.Instance.StartMatchMulti(GlobalVar.Uname);
        }

        public void TryStoryMode()
        {
            HideRoomTypeUi();
            LogicProxy.Instance.StartStoryMode(GlobalVar.Uname);
        }
        
        

        public void UpdateUserInfo()
        {
            userInfoRect.UpdateInfo(
                AssetConstUtil.GetUserIcon(GlobalVar.Uface),
                GlobalVar.Unick,
                GlobalVar.Ulevel,
                GlobalVar.Uexp);
            coinBarInfo.SetValue(GlobalVar.Ucoin);
            diamondBarInfo.SetValue(GlobalVar.Udiamond);
        }
    }
}