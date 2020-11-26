using PurificationPioneer.Const;
using PurificationPioneer.Global;
using PurificationPioneer.Network.ProtoGen;
using PurificationPioneer.Network.Proxy;
using ReadyGamerOne.Common;

namespace PurificationPioneer.View
{
	public partial class HomePanel
	{
		private HomePanelScript script;
		partial void OnLoad()
		{
			//do any thing you want
			script = m_TransFrom.GetComponent<HomePanelScript>();
			
			//登陆逻辑服务器
			LogicProxy.Instance.Login();
			
			script.playBtn.onClick.AddListener(() =>
			{
				LogicProxy.Instance.StartMatch(
					GlobalVar.uname);
			});
		}

		protected override void OnAddListener()
		{
			base.OnAddListener();
			CEventCenter.AddListener<StartMatchRes>(Message.OnStartMatch,OnStartMatch);
			CEventCenter.AddListener(Message.OnAddPlayer,OnAddPlayer);
			CEventCenter.AddListener(Message.OnRemovePlayer,OnRemovePlayer);
			CEventCenter.AddListener(Message.OnStopMatch, OnStopMatch);
		}

		protected override void OnRemoveListener()
		{
			base.OnRemoveListener();
			CEventCenter.RemoveListener<StartMatchRes>(Message.OnStartMatch,OnStartMatch);
			CEventCenter.RemoveListener(Message.OnAddPlayer,OnAddPlayer);
			CEventCenter.RemoveListener(Message.OnRemovePlayer,OnRemovePlayer);
			CEventCenter.RemoveListener(Message.OnStopMatch, OnStopMatch);
		}

		private void OnStopMatch()
		{
			script.matchUi.StopMatch();
		}

		private void OnRemovePlayer()
		{
			script.matchUi.RemovePlayer();
		}

		private void OnAddPlayer()
		{
			script.matchUi.AddPlayer();
		}

		private void OnStartMatch(StartMatchRes res)
		{
			script.matchUi.StartMatch(res.current,res.max);
		}
	}
}
