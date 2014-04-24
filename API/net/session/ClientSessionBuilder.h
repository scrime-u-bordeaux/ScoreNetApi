#pragma once
#include "ClientSession.h"


class ClientSessionBuilder : public hasId, public hasName
{
	public:
		// à mettre en privé, devrait être appelé uniquement par ::list()
		ClientSessionBuilder(int32_t id,
							 std::string sessionName,
							 std::string masterName,
							 std::string masterIp,
							 int masterInputPort,
							 std::string localName):
			hasId(id),
			hasName(sessionName),
			_remoteMaster(new RemoteMaster(0,
										   masterName,
										   masterIp,
										   masterInputPort))
		{
			session._localClient.reset(
						new LocalClient(std::unique_ptr<OscReceiver>(new OscReceiver(8900)),
										-1,
										localName));

			session.getLocalClient().receiver().addHandler("/session/clientNameIsTaken",
								 &ClientSessionBuilder::handle__session_clientNameIsTaken, this);

			session.getLocalClient().receiver().addHandler("/session/setClientId",
								 &ClientSessionBuilder::handle__session_setClientId, this);
			session.getLocalClient().receiver().addHandler("/session/update/group",
								 &ClientSessionBuilder::handle__session_update_group, this);

			session.getLocalClient().receiver().addHandler("/session/isReady",
								 &ClientSessionBuilder::handle__session_isReady, this);

			session.getLocalClient().receiver().run();
		}

		virtual ~ClientSessionBuilder() = default;
		ClientSessionBuilder(const ClientSessionBuilder& s) = default;
		ClientSessionBuilder(ClientSessionBuilder&& s) = default;

		/**** Handlers ****/
		// /session/update/group idSession idGroupe nomGroupe isMute
		void handle__session_update_group(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession, idGroupe;
			bool isMute;
			const char* s;

			args >> idSession >> idGroupe >> s >> isMute >> osc::EndMessage;

			if(idSession == getId())
			{
				auto& g = session.private__createGroup(idGroupe, std::string(s));
				isMute? g.mute() : g.unmute();
			}
		}

		// /session/isReady idSession
		void handle__session_isReady(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;

			args >> idSession >> osc::EndMessage;

			if(getId() == idSession) _isReady = true;
		}

		// /session/clientNameIsTaken idSession nom
		void handle__session_clientNameIsTaken(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession;
			const char* cname;

			args >> idSession >> cname >> osc::EndMessage;
			std::string name(cname);

			if(idSession == getId() && session.getLocalClient().getName() == name)
			{
				session.getLocalClient().setName(session.getLocalClient().getName() + "_");
				join();
			}
		}

		// /session/setClientId idSession idClient
		void handle__session_setClientId(osc::ReceivedMessageArgumentStream args)
		{
			osc::int32 idSession, idClient;

			args >> idSession >> idClient >> osc::EndMessage;

			if(idSession == getId())
			{
				session.getLocalClient().setId(idClient);
			}
		}


		/**** Connection ****/
		// Si on veut, on récupère la liste des sessions dispo sur le réseau
		static std::vector<ClientSessionBuilder> list(std::string localName)
		{
			std::vector<ClientSessionBuilder> v;

			v.emplace_back(3453525,
						   "Session Electro-pop",
						   "Machine du compositeur",
						   "127.0.0.1",
						   12345,
						   localName);

			v.emplace_back(21145,
						   "Session test",
						   "Machine drôle",
						   "92.91.90.89",
						   8908,
						   localName);

			return v;
		}


		// 2. On appelle "join" sur celle qu'on désire rejoindre.
		void join()
		{
			_remoteMaster->send("/session/connect" ,
								getId(),
								session.getLocalClient().getName().c_str(),
								_localClientPort);

			// Envoyer message de connection au serveur.
			// Il va construire peu à peu session.
			//
			// Eventuellement :
			//	/session/clientNameIsTaken
			//
			// Puis :
			//	/session/setClientId
			//	/session/update/group
			//
			// Enfin :
			//	/session/isReady
		}

		// 3. On teste si la construction est terminée
		// Construction : Envoyer tous les groupes.
		bool isReady()
		{
			return _isReady;
		}

		// 4. A appeler uniquement quand isReady
		ClientSession&& getBuiltSession()
		{
			if(!_isReady) throw "Is not ready";

			// Construction de l'objet Session.
			session.setId(getId());
			session.setName(getName());

			session._remoteMaster = std::move(_remoteMaster);

			return std::move(session);
		}

	private:
		std::unique_ptr<RemoteMaster> _remoteMaster{nullptr};
		ClientSession session{""};

		int _localClientPort{8900};

		bool _isReady = false;
		std::unique_ptr<OscReceiver> _receiver{nullptr};
};
