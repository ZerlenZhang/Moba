﻿using PurificationPioneer.Script;
using UnityEngine;
using UnityEngine.Assertions;

namespace PurificationPioneer.Scriptable
{
    [CreateAssetMenu(fileName = "NewCharacterConfig", menuName = "净化先锋/CharacterConfig", order = 0)]
    public class CharacterConfigAsset : ScriptableObject
    {
#if UNITY_EDITOR
        [SerializeField] private string 备注;

        [Space]
#endif
        [Header("常规")]
        public int characterId;
        public GameObject prefab;
        
        [Header("基础数值")]
        public int baseHp;
        public int increaseHp;
        public int baseAttack;
        public int increaseAttack;
        public int baseDefence;
        public int increaseDefence;
        public int basePaintEfficiency;
        public int increasePaintEfficiency;

        [Header("技能配置")]
        [SerializeField]private SkillConfigAsset firstSkill;
        [SerializeField]private SkillConfigAsset secondSkill;

        [Header("世界观故事")]
        public string stroy0;
        public string stroy1;
        public string stroy2;
        public string stroy3;
        public string stroy4;
        
        
        public IPpController InstantiateAndInitialize(int seatId, Vector3 worldPosition, Transform parent=null)
        {
            Assert.IsTrue(prefab);
            
            var characterObj = Object.Instantiate(prefab, parent);
            Assert.IsTrue(characterObj);
            
            var ppController = characterObj.GetComponent<IPpController>();
            Assert.IsNotNull(ppController);
            
            //init settings
            characterObj.transform.position = worldPosition;
            ppController.InitCharacterController(seatId, worldPosition, this);
            
            return ppController;
        }
    }
}