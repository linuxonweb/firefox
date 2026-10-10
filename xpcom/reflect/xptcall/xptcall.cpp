/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* entry point wrappers. */

#include "xptcprivate.h"

using namespace mozilla;

NS_IMETHODIMP
nsXPTCStubBase::QueryInterface(REFNSIID aIID, void** aInstancePtr) {
  if (aIID.Equals(mEntry->IID())) {
    NS_ADDREF_THIS();
    *aInstancePtr = static_cast<nsISupports*>(this);
    return NS_OK;
  }

  return mOuter->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsXPTCStubBase::AddRef() { return mOuter->AddRef(); }

NS_IMETHODIMP_(MozExternalRefCountType)
nsXPTCStubBase::Release() { return mOuter->Release(); }

EXPORT_XPCOM_API(nsresult)
NS_GetXPTCallStub(REFNSIID aIID, nsIXPTCProxy* aOuter,
                  nsISomeInterface** aResult) {
  if (NS_WARN_IF(!aOuter) || NS_WARN_IF(!aResult)) return NS_ERROR_INVALID_ARG;

  const nsXPTInterfaceInfo* iie = nsXPTInterfaceInfo::ByIID(aIID);
  if (!iie || iie->IsBuiltinClass()) {
    return NS_ERROR_FAILURE;
  }

#if defined(__wasm__) && defined(__linux__) && !defined(__wasi__)
  *aResult = nullptr;
  auto* stub = new nsXPTCStubBase(aOuter, iie);
  nsresult rv = WasmInitXPTCStub(stub);
  if (NS_FAILED(rv)) {
    delete stub;
    return rv;
  }
  *aResult = stub;
#else
  *aResult = new nsXPTCStubBase(aOuter, iie);
#endif
  return NS_OK;
}

EXPORT_XPCOM_API(void)
NS_DestroyXPTCallStub(nsISomeInterface* aStub) {
  nsXPTCStubBase* stub = static_cast<nsXPTCStubBase*>(aStub);
  delete (stub);
}

EXPORT_XPCOM_API(size_t)
NS_SizeOfIncludingThisXPTCallStub(const nsISomeInterface* aStub,
                                  mozilla::MallocSizeOf aMallocSizeOf) {
  // We could cast aStub to nsXPTCStubBase, but that class doesn't seem to own
  // anything, so just measure the size of the object itself.
#if defined(__wasm__) && defined(__linux__) && !defined(__wasi__)
  return aMallocSizeOf(aStub) +
         aMallocSizeOf(static_cast<const nsXPTCStubBase*>(aStub)->mWasmVTable);
#else
  return aMallocSizeOf(aStub);
#endif
}
