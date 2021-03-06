#ifndef FAMILY_H__
#define FAMILY_H__

#include <map>
#include <string>
#include <memory>
#include "Signature.h"
#include "Identity.h"

namespace i2p
{
namespace data
{
	class Families
	{
		public:

			Families ();
			~Families ();
			void LoadCertificates ();
			bool VerifyFamily (const std::string& family, const IdentHash& ident,
				const char * signature, const char * key = nullptr);

		private:

			void LoadCertificate (const std::string& filename);

		private:

			std::map<std::string, std::shared_ptr<i2p::crypto::Verifier> > m_SigningKeys;
	};

	std::string CreateFamilySignature (const std::string& family, const IdentHash& ident);
	// return base64 signature of empty string in case of failure
}
}

#endif
