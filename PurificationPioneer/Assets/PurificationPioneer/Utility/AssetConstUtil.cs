﻿using ReadyGamerOne.MemorySystem;
using UnityEngine;

namespace PurificationPioneer.Utility
{
    public partial class AssetConstUtil
    {
        private const string HeroIconKey = "HeroIcon";
        private const string UserIconKey = "Avatar";
        private const string HeroGameObjectKey = "Character";

        public static string GetHeroGameObjectKey(int heroId)
        {
            return $"{HeroGameObjectKey}{heroId}";
        }
        
        public static Sprite GetHeroIcon(int heroId)
        {
            return ResourceMgr.GetAsset<Sprite>($"{HeroIconKey}{heroId}");
        }

        public static Sprite GetUserIcon(int uface)
        {
            return ResourceMgr.GetAsset<Sprite>($"{UserIconKey}{uface}");
        }
    }
}