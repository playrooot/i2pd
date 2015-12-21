#ifndef DESTINATION_H__
#define DESTINATION_H__

#include <thread>
#include <mutex>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <functional>
#include <boost/asio.hpp>
#include "Identity.h"
#include "TunnelPool.h"
#include "Crypto.h"
#include "LeaseSet.h"
#include "Garlic.h"
#include "NetDb.h"
#include "Streaming.h"
#include "Datagram.h"

namespace i2p
{
namespace client
{
	const uint8_t PROTOCOL_TYPE_STREAMING = 6;
	const uint8_t PROTOCOL_TYPE_DATAGRAM = 17;
	const uint8_t PROTOCOL_TYPE_RAW = 18;	
	const int PUBLISH_CONFIRMATION_TIMEOUT = 5; // in seconds
	const int LEASESET_REQUEST_TIMEOUT = 5; // in seconds
	const int MAX_LEASESET_REQUEST_TIMEOUT = 40; // in seconds
	const int DESTINATION_CLEANUP_TIMEOUT = 20; // in minutes 
	const unsigned int MAX_NUM_FLOODFILLS_PER_REQUEST = 7;
	
	// I2CP
	const char I2CP_PARAM_INBOUND_TUNNEL_LENGTH[] = "inbound.length";
	const int DEFAULT_INBOUND_TUNNEL_LENGTH = 3;
	const char I2CP_PARAM_OUTBOUND_TUNNEL_LENGTH[] = "outbound.length";
	const int DEFAULT_OUTBOUND_TUNNEL_LENGTH = 3;
	const char I2CP_PARAM_INBOUND_TUNNELS_QUANTITY[] = "inbound.quantity";
	const int DEFAULT_INBOUND_TUNNELS_QUANTITY = 5;
	const char I2CP_PARAM_OUTBOUND_TUNNELS_QUANTITY[] = "outbound.quantity";
	const int DEFAULT_OUTBOUND_TUNNELS_QUANTITY = 5;
	const char I2CP_PARAM_EXPLICIT_PEERS[] = "explicitPeers";
	const int STREAM_REQUEST_TIMEOUT = 60; //in seconds

	typedef std::function<void (std::shared_ptr<i2p::stream::Stream> stream)> StreamRequestComplete;

	class ClientDestination: public i2p::garlic::GarlicDestination,
		public std::enable_shared_from_this<ClientDestination>
	{
		typedef std::function<void (std::shared_ptr<i2p::data::LeaseSet> leaseSet)> RequestComplete;
		// leaseSet = nullptr means not found
		struct LeaseSetRequest
		{
			LeaseSetRequest (boost::asio::io_service& service): requestTime (0), requestTimeoutTimer (service) {};
			std::set<i2p::data::IdentHash> excluded;
			uint64_t requestTime;
			boost::asio::deadline_timer requestTimeoutTimer;
			RequestComplete requestComplete;
		};	
		
		
		public:

			ClientDestination (const i2p::data::PrivateKeys& keys, bool isPublic, const std::map<std::string, std::string> * params = nullptr);
			~ClientDestination ();	

			virtual void Start ();
			virtual void Stop ();
			bool IsRunning () const { return m_IsRunning; };
			boost::asio::io_service& GetService () { return m_Service; };
			std::shared_ptr<i2p::tunnel::TunnelPool> GetTunnelPool () { return m_Pool; }; 
			bool IsReady () const { return m_LeaseSet && m_LeaseSet->HasNonExpiredLeases () && m_Pool->GetOutboundTunnels ().size () > 0; };
			std::shared_ptr<const i2p::data::LeaseSet> FindLeaseSet (const i2p::data::IdentHash& ident);
			bool RequestDestination (const i2p::data::IdentHash& dest, RequestComplete requestComplete = nullptr);
			void CancelDestinationRequest (const i2p::data::IdentHash& dest);	
			
			// streaming
			std::shared_ptr<i2p::stream::StreamingDestination> CreateStreamingDestination (int port); // additional
			std::shared_ptr<i2p::stream::StreamingDestination> GetStreamingDestination (int port = 0) const;
			// following methods operate with default streaming destination
			void CreateStream (StreamRequestComplete streamRequestComplete, const i2p::data::IdentHash& dest, int port = 0);
			std::shared_ptr<i2p::stream::Stream> CreateStream (std::shared_ptr<const i2p::data::LeaseSet> remote, int port = 0);
			void AcceptStreams (const i2p::stream::StreamingDestination::Acceptor& acceptor);
			void StopAcceptingStreams ();
			bool IsAcceptingStreams () const;
			
			// datagram
			i2p::datagram::DatagramDestination * GetDatagramDestination () const { return m_DatagramDestination; };
			i2p::datagram::DatagramDestination * CreateDatagramDestination ();

			// implements LocalDestination
			const i2p::data::PrivateKeys& GetPrivateKeys () const { return m_Keys; };
			const uint8_t * GetEncryptionPrivateKey () const { return m_EncryptionPrivateKey; };
			const uint8_t * GetEncryptionPublicKey () const { return m_EncryptionPublicKey; };
			
			// implements GarlicDestination
			std::shared_ptr<const i2p::data::LeaseSet> GetLeaseSet ();
			std::shared_ptr<i2p::tunnel::TunnelPool> GetTunnelPool () const { return m_Pool; }
			void HandleI2NPMessage (const uint8_t * buf, size_t len, std::shared_ptr<i2p::tunnel::InboundTunnel> from);

			// override GarlicDestination
			bool SubmitSessionKey (const uint8_t * key, const uint8_t * tag);
			void ProcessGarlicMessage (std::shared_ptr<I2NPMessage> msg);
			void ProcessDeliveryStatusMessage (std::shared_ptr<I2NPMessage> msg);	
			void SetLeaseSetUpdated ();

			// I2CP
			void HandleDataMessage (const uint8_t * buf, size_t len);

		private:
				
			void Run ();			
			void UpdateLeaseSet ();
			void Publish ();
			void HandlePublishConfirmationTimer (const boost::system::error_code& ecode);
			void HandleDatabaseStoreMessage (const uint8_t * buf, size_t len);
			void HandleDatabaseSearchReplyMessage (const uint8_t * buf, size_t len);
			void HandleDeliveryStatusMessage (std::shared_ptr<I2NPMessage> msg);		

			void RequestLeaseSet (const i2p::data::IdentHash& dest, RequestComplete requestComplete);
			bool SendLeaseSetRequest (const i2p::data::IdentHash& dest, std::shared_ptr<const i2p::data::RouterInfo>  nextFloodfill, std::shared_ptr<LeaseSetRequest> request);	
			void HandleRequestTimoutTimer (const boost::system::error_code& ecode, const i2p::data::IdentHash& dest);
			void HandleCleanupTimer (const boost::system::error_code& ecode);
			void CleanupRemoteLeaseSets ();
			void PersistTemporaryKeys ();			

		private:

			volatile bool m_IsRunning;
			std::thread * m_Thread;	
			boost::asio::io_service m_Service;
			boost::asio::io_service::work m_Work;
			i2p::data::PrivateKeys m_Keys;
			uint8_t m_EncryptionPublicKey[256], m_EncryptionPrivateKey[256];
			std::map<i2p::data::IdentHash, std::shared_ptr<i2p::data::LeaseSet> > m_RemoteLeaseSets;
			std::map<i2p::data::IdentHash, std::shared_ptr<LeaseSetRequest> > m_LeaseSetRequests;

			std::shared_ptr<i2p::tunnel::TunnelPool> m_Pool;
			std::shared_ptr<i2p::data::LeaseSet> m_LeaseSet;
			bool m_IsPublic;
			uint32_t m_PublishReplyToken;
			std::set<i2p::data::IdentHash> m_ExcludedFloodfills; // for publishing
			
			std::shared_ptr<i2p::stream::StreamingDestination> m_StreamingDestination; // default
			std::map<uint16_t, std::shared_ptr<i2p::stream::StreamingDestination> > m_StreamingDestinationsByPorts;
			i2p::datagram::DatagramDestination * m_DatagramDestination;
	
			boost::asio::deadline_timer m_PublishConfirmationTimer, m_CleanupTimer;

		public:
			
			// for HTTP only
			int GetNumRemoteLeaseSets () const { return m_RemoteLeaseSets.size (); };
	};	
}	
}	

#endif
