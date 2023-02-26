
//
// iiswebsocket.cpp
// 
// Author:
//     Brian Sullender
//     SULLE WAREHOUSE LLC
// 
// Description:
//     The primary source file for the IIS WebSocket server interface.
//

#include "iiswebsocket.h"
using namespace IISWebSocketServer;

// The headers a client must send to create a WebSocket connection
static CHAR* requiredHeaders[] = { "Connection", "Upgrade" };

// The values the required headers must have, respectively
static CHAR* requiredHeadersValues[] = { "Upgrade", "websocket" };

// Optional headers a client may send for the connection
static CHAR* optionalHeaders[] = { "Sec-WebSocket-Version", "Sec-WebSocket-Key", "Sec-WebSocket-Protocol", "Host", "User-Agent" };

// Generate WebSocket masking key
void WebSocketGenerateMaskingKey(CHAR pMaskingKey[4])
{
	DWORD key = rand();
	pMaskingKey[0] = key & 0xFF;
	pMaskingKey[1] = (key >> 8) & 0xFF;
	pMaskingKey[2] = (key >> 16) & 0xFF;
	pMaskingKey[3] = (key >> 24) & 0xFF;
}

void IISWebSocketServer::PrintLastError(DWORD errorCode, CHAR* des, size_t desLen, CHAR* action, bool append)
{
	size_t offset;

	if (!append) {
		offset = 0;
	}
	else {
		offset = strlen(des);
	}

	if (offset > desLen) { // Buffer too small!
		return;
	}

	if (errorCode == ERROR_INVALID_OPERATION) {
		sprintf_s(&des[offset], desLen - offset, "%s %s", action, "failed; ERROR_INVALID_OPERATION\n");
	}
	else if (errorCode == ERROR_NOT_ENOUGH_MEMORY) {
		sprintf_s(&des[offset], desLen - offset, "%s %s", action, "failed; ERROR_NOT_ENOUGH_MEMORY\n");
	}
	else if (errorCode == ERROR_INVALID_PARAMETER) {
		sprintf_s(&des[offset], desLen - offset, "%s %s", action, "failed; ERROR_INVALID_PARAMETER\n");
	}
	else if (errorCode == ERROR_INSUFFICIENT_BUFFER) {
		sprintf_s(&des[offset], desLen - offset, "%s %s", action, "failed; ERROR_INSUFFICIENT_BUFFER\n");
	}
	else {
		sprintf_s(&des[offset], desLen - offset, "%s %s 0x%X%s", action, "failed; error code =", errorCode, "\n");
	}
}

DWORD WebSocketServer::Initialize()
{
	// Set class data to zero
	memset(this, 0, sizeof(WebSocketServer));

	// Set default error code
	this->ErrorCode = S_OK;

	// We will be ready to start receiving frames
	this->Stream.bQueuing = true;

	// Set default max payload length of 4 GB (Gibibyte)
	this->MaxPayloadLength = 0x400000;

	// Set the length of the description buffer
	this->ErrorBufferLength = 0x1000;

	this->Stream.pFrameBuffer = (CHAR*)malloc(0x100);
	if (this->Stream.pFrameBuffer == NULL) {
		this->ErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		goto exit;
	}

	// Allocate memory for the description buffer
	this->ErrorDescription = (CHAR*)malloc(this->ErrorBufferLength);
	if (this->ErrorDescription == NULL) {
		free(this->Stream.pFrameBuffer);
		this->ErrorCode = ERROR_NOT_ENOUGH_MEMORY;
	}

exit:

	// Return error code
	return this->ErrorCode;
}

HRESULT WebSocketServer::PerformHandshake(IHttpContext* pHttpContext)
{
	// Returned error code
	HRESULT errorCode;

	// Variables for getting headers values
	PCSTR pHeaderValuePointer;
	USHORT headerValueLength;
	CHAR* pHeaderValueBuffer;

	// Handle to generate the necessary headers for the handshake
	WEB_SOCKET_HANDLE ServerHandle;

	// Additional headers to send for the handshake (NOTE you do not need to free this memory)
	WEB_SOCKET_HTTP_HEADER* pAdditionalHeaders;
	ULONG AdditionalHeaderCount;

	// Set default pointer values
	pHeaderValueBuffer = NULL;
	ServerHandle = NULL;
	this->pRequestHeaders = NULL;

	// Default success code
	errorCode = S_OK;

	// Set pHttpContext in out class
	this->pHttpContext = pHttpContext;

	// Get a pointer to the connection
	this->pHttpConnection = pHttpContext->GetConnection();
	if (this->pHttpConnection == NULL) {
		errorCode = ERROR_INVALID_PARAMETER;
		strcpy_s(this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::PerformHandshake() IHttpContext::GetConnection() returned NULL");
		goto exit;
	}

	// Create a buffer for the headers value
	// NOTE: This is only for checking the value string
	pHeaderValueBuffer = (CHAR*)malloc(0x1000);
	if (pHeaderValueBuffer == NULL) {
		errorCode = ERROR_NOT_ENOUGH_MEMORY;
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::PerformHandshake()");
		goto exit;
	}

	// Create an array of WEB_SOCKET_HTTP_HEADER for our call to WebSocketBeginServerHandshake
	this->pRequestHeaders = (WEB_SOCKET_HTTP_HEADER*)malloc(sizeof(WEB_SOCKET_HTTP_HEADER) * (ARRAYSIZE(requiredHeaders) + ARRAYSIZE(optionalHeaders)));
	if (this->pRequestHeaders == NULL) {
		errorCode = ERROR_NOT_ENOUGH_MEMORY;
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::PerformHandshake()");
		goto exit;
	}

	// Get a pointer to the response headers
	this->pHttpResponse = pHttpContext->GetResponse();
	if (this->pHttpResponse == NULL) {
		errorCode = ERROR_INVALID_PARAMETER;
		strcpy_s(this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::PerformHandshake() IHttpContext::GetResponse() returned NULL");
		goto exit;
	}

	// Get a pointer to the request headers
	this->pHttpRequest = pHttpContext->GetRequest();
	if (this->pHttpRequest == NULL) {
		errorCode = ERROR_INVALID_PARAMETER;
		strcpy_s(this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::PerformHandshake() IHttpContext::GetRequest() returned NULL");
		goto exit;
	}

	// Get and check the values of the required headers
	for (int i = 0; i < ARRAYSIZE(requiredHeaders); i++)
	{
		// Get a pointer to the required header value
		pHeaderValuePointer = this->pHttpRequest->GetHeader(requiredHeaders[i], &headerValueLength);
		if ((headerValueLength == 0) || (headerValueLength >= 0x1000) || (pHeaderValuePointer == NULL)) {
			errorCode = ERROR_INVALID_PARAMETER;

			// Print pointer and length on failure
			sprintf_s(this->ErrorDescription, this->ErrorBufferLength, "%s%s%s%p%s%u%s",
				"WebSocketServer::PerformHandshake() required header '",
				requiredHeaders[i],
				"' missing or invalid. Pointer(",
				pHeaderValuePointer,
				") Length(",
				headerValueLength,
				").");

			goto exit;
		}

		// Create a temporary NULL terminated version of the value
		memcpy(pHeaderValueBuffer, pHeaderValuePointer, headerValueLength);
		pHeaderValueBuffer[headerValueLength] = 0;

		// Some clients may send a comma separated list for the 'Connection' header
		// For example, Firefox sends the following "keep-alive, Upgrade"
		if (_stricmp("Connection", requiredHeaders[i]) == 0)
		{
			// Set to true if 'Upgrade' is found
			bool IsConnectionUpgrade = false;

			// Allocate a buffer to create a NULL terminated string of each comma separated item
			CHAR* pConnectionValueBuffer = (CHAR*)malloc(0x1000);

			// Set the initial parser pointer
			CHAR* pValueSource = pHeaderValueBuffer;

			// Parse the string for each comma separated item
		ParseConnectionValue:

			// Skip spaces
			while (*pValueSource == ' ') {
				pValueSource++;
			}

			DWORD i = 0;
			while ((*pValueSource != 0) && (*pValueSource != ','))
			{
				pConnectionValueBuffer[i] = *pValueSource;
				pValueSource++;
				i++;
			}

			// Set NULL character
			pConnectionValueBuffer[i] = 0;

			// Check for 'Upgrade' string
			if (_stricmp("Upgrade", pConnectionValueBuffer) == 0) {
				IsConnectionUpgrade = true;
			}
			else
			{
				// Are we done parsing?
				if (*pValueSource != 0) {
					if (*pValueSource == ',') {
						pValueSource++;
					}
					goto ParseConnectionValue;
				}
			}

			// Free the connection value buffer
			free(pConnectionValueBuffer);

			// We have finished parsing, check if the 'Upgrade' parameter was found
			if (IsConnectionUpgrade == false) {
				errorCode = ERROR_INVALID_PARAMETER;

				// Print header value on failure
				sprintf_s(this->ErrorDescription, this->ErrorBufferLength, "%s%s%s%s%s",
					"WebSocketServer::PerformHandshake() required header '",
					requiredHeaders[i],
					"' has an invalid value \"",
					pHeaderValueBuffer,
					"\".");

				goto exit;
			}
		}
		else
		{
			// Check the value is correct for the respective header
			if (_stricmp(pHeaderValueBuffer, requiredHeadersValues[i]) != 0) {
				errorCode = ERROR_INVALID_PARAMETER;

				// Print header value on failure
				sprintf_s(this->ErrorDescription, this->ErrorBufferLength, "%s%s%s%s%s",
					"WebSocketServer::PerformHandshake() required header '",
					requiredHeaders[i],
					"' has an invalid value \"",
					pHeaderValueBuffer,
					"\".");

				goto exit;
			}
		}

		// Add the header to our array
		this->pRequestHeaders[i].pcName = requiredHeaders[i];
		this->pRequestHeaders[i].ulNameLength = (ULONG)strlen(requiredHeaders[i]);
		this->pRequestHeaders[i].pcValue = (PCHAR)pHeaderValuePointer;
		this->pRequestHeaders[i].ulValueLength = headerValueLength;
	}

	// Set the count to (required headers)
	this->RequestHeadersCount = ARRAYSIZE(requiredHeaders);

	// Get the optional headers and their values
	for (int i = 0; i < ARRAYSIZE(optionalHeaders); i++)
	{
		// Get a pointer to the optional header value
		pHeaderValuePointer = this->pHttpRequest->GetHeader(optionalHeaders[i], &headerValueLength);
		if ((headerValueLength == 0) || (headerValueLength >= 0x1000) || (pHeaderValuePointer == NULL)) {
			continue;
		}

		// Add the header to our array
		this->pRequestHeaders[this->RequestHeadersCount].pcName = optionalHeaders[i];
		this->pRequestHeaders[this->RequestHeadersCount].ulNameLength = (ULONG)strlen(optionalHeaders[i]);
		this->pRequestHeaders[this->RequestHeadersCount].pcValue = (PCHAR)pHeaderValuePointer;
		this->pRequestHeaders[this->RequestHeadersCount].ulValueLength = headerValueLength;

		// Increase our header count
		this->RequestHeadersCount++;
	}

	// Create the necessary handle for our call to WebSocketBeginServerHandshake
	errorCode = WebSocketCreateServerHandle(NULL, 0, &ServerHandle);
	if (errorCode != S_OK) {
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketCreateServerHandle()");
		goto exit;
	}

	// Generate the necessary headers for the handshake
	errorCode = WebSocketBeginServerHandshake(ServerHandle, NULL, NULL, 0,
		pRequestHeaders, RequestHeadersCount, &pAdditionalHeaders, &AdditionalHeaderCount);
	if (errorCode != S_OK) {
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketBeginServerHandshake()");
		goto exit;
	}

	// End the handshake, ignore the return value (we just wanted the necessary headers), this has no real effect
	WebSocketEndServerHandshake(ServerHandle);

	// Clear the existing response.
	this->pHttpResponse->Clear();

	// Set the 'Status' header
	errorCode = this->pHttpResponse->SetStatus(101, "Switching Protocols");
	if (errorCode != S_OK) {
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "IHttpResponse::SetStatus()");
		goto exit;
	}

	// Add the clients headers to our response
	for (ULONG i = 0; i < this->RequestHeadersCount; i++)
	{
		this->pHttpResponse->SetHeader(this->pRequestHeaders[i].pcName,
			this->pRequestHeaders[i].pcValue, (USHORT)this->pRequestHeaders[i].ulValueLength, TRUE);
	}

	// Add the additional handshake headers to our response
	for (ULONG i = 0; i < AdditionalHeaderCount; i++)
	{
		this->pHttpResponse->SetHeader(pAdditionalHeaders[i].pcName,
			pAdditionalHeaders[i].pcValue, (USHORT)pAdditionalHeaders[i].ulValueLength, TRUE);
	}

	// Number of bytes sent to the client
	DWORD cbSent = 0;

	// Asynchronous completion pending (not used)
	BOOL fCompletionExpected = false;

	// Send our handshake response to the client
	errorCode = this->pHttpResponse->Flush(FALSE, TRUE, &cbSent, &fCompletionExpected);
	if (errorCode != S_OK) {
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "IHttpResponse::Flush()");
	}

	// Disbale buffering
	this->pHttpResponse->DisableBuffering();

exit:

	// Free resources and return the error code

	if (pHeaderValueBuffer) {
		free(pHeaderValueBuffer);
	}

	if (ServerHandle) {
		WebSocketDeleteHandle(ServerHandle);
	}

	// We only free our headers if our handshake failed
	if (errorCode != S_OK)
	{
		if (this->pRequestHeaders) {
			free(this->pRequestHeaders);
			this->pRequestHeaders = NULL;
		}
	}

	// Set error code
	this->ErrorCode = errorCode;

	// Return error code
	return errorCode;
}

bool ParseWebSocketFrame(UCHAR* pBuffer, DWORD dwLength, WEB_SOCKET_FRAME* pOutFrame)
{
	// Minimal size is 2 bytes for a WebSocket frame
	pOutFrame->FrameSize = 2;
	if (dwLength < 2) {
		return false;
	}

	// Get the Opcode and FIN from the 1st byte
	pOutFrame->Opcode = pBuffer[0] & 0x0F;
	pOutFrame->FIN = pBuffer[0] & 0x80;

	// Get the Payload length and Mask boolean from the 2nd byte
	unsigned long long payloadLength = pBuffer[1] & 0x7F;
	pOutFrame->bMask = pBuffer[1] & 0x80;

	// Calculate the size of the full WebSocket frame
	if (pOutFrame->bMask) {
		pOutFrame->FrameSize += 4;
	}
	if (payloadLength == 126) {
		pOutFrame->FrameSize += 2;
	}
	else if (payloadLength == 127) {
		pOutFrame->FrameSize += 8;
	}

	// Return false if the buffer doesn't contain all of the frame
	if (dwLength < pOutFrame->FrameSize) {
		return false;
	}

	// This will be our index to get the masking key
	int maskingKeyIndex = 2;

	// Get the actual Payload length and Masking key index
	if (payloadLength == 126)
	{
		// Payload length is stored in the next 2 bytes
		payloadLength = ((unsigned long long)pBuffer[2] << 8) + pBuffer[3];
		maskingKeyIndex = 4;
	}
	else if (payloadLength == 127)
	{
		// Payload length is stored in the next 8 bytes
		payloadLength =
			((unsigned long long)pBuffer[2] << 56) +
			((unsigned long long)pBuffer[3] << 48) +
			((unsigned long long)pBuffer[4] << 40) +
			((unsigned long long)pBuffer[5] << 32) +
			((unsigned long long)pBuffer[6] << 24) +
			((unsigned long long)pBuffer[7] << 16) +
			((unsigned long long)pBuffer[8] << 8) +
			(unsigned long long)pBuffer[9];
		maskingKeyIndex = 10;
	}

	// Set the Payload length
	pOutFrame->PayloadLength = payloadLength;

	// Read the Masking key
	if (pOutFrame->bMask)
	{
		for (int i = 0; i < 4; i++)
		{
			pOutFrame->MaskingKey[i] = pBuffer[maskingKeyIndex + i];
		}
	}

	// We have parsed the full frame
	return true;
}

DWORD WebSocketServer::Receive(void* pBuffer, DWORD dwBufferLength, DWORD* pdwBytesReceived, IIS_WEB_SOCKET_BUFFER_TYPE* pBufferType)
{
	DWORD errorCode;
	DWORD dwBytesReceived;
	BOOL fCompletionPending;
	DWORD dwMaxReceive;

	// Set success
	errorCode = S_OK;

	// Set defaults
	dwBytesReceived = 0;
	fCompletionPending = FALSE;

	// pBuffer, pdwBytesReceived and pBufferType must be valid pointers
	if ((pBuffer == NULL) || (pdwBytesReceived == NULL) || (pBufferType == NULL)) {
		errorCode = ERROR_INVALID_PARAMETER;
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::Receive() 'input paramter'");
		goto exit;
	}

	// Ensure *pdwBytesReceived = 0
	*pdwBytesReceived = 0;

	// Are we queuing a new frame?
	if (this->Stream.bQueuing)
	{
		// Reset dwReceivedSize
		this->Stream.dwReceivedSize = 0;

		// Parse the frame
		while (!ParseWebSocketFrame((UCHAR*)this->Stream.pFrameBuffer, this->Stream.dwReceivedSize, &this->WebSocketFrame))
		{
			// Reset parameters
			dwBytesReceived = 0;
			fCompletionPending = FALSE;

			// Receive data
			errorCode = this->pHttpRequest->ReadEntityBody(this->Stream.pFrameBuffer + this->Stream.dwReceivedSize, this->WebSocketFrame.FrameSize - this->Stream.dwReceivedSize, FALSE, &dwBytesReceived, &fCompletionPending);
			if ((errorCode != S_OK) && (HRESULT_CODE(errorCode) != ERROR_MORE_DATA) && (HRESULT_CODE(errorCode) != ERROR_HANDLE_EOF)) {
				PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "ReadEntityBody() 'WebSocket frame'");
				goto exit;
			}

			// Reset error code because it could be ERROR_MORE_DATA or ERROR_HANDLE_EOF
			errorCode = S_OK;

			// Add bytes received to our total
			this->Stream.dwReceivedSize += dwBytesReceived;
		}

		// Check if the payload will exceed the maximum length set by the server
		if (this->WebSocketFrame.PayloadLength > this->MaxPayloadLength) {
			errorCode = ERROR_INVALID_BLOCK_LENGTH;
			strcpy_s(this->ErrorDescription, this->ErrorBufferLength, "Received WebSocket `PayloadLength` exceeded `MaxPayloadLength`");
			goto exit;
		}

		// Reset parameters
		dwBytesReceived = 0;
		fCompletionPending = FALSE;

		// Setup our stream parameters
		this->Stream.qwPayloadRemaining = this->WebSocketFrame.PayloadLength;

		// Reset masking key index
		this->Stream.mkI = 0;

		// Set queuing to false until payload is received and transferred
		this->Stream.bQueuing = false;
	}

receiveLoop: // Only used for "Connection close", "Ping" and "Pong" frames

	// Determine the max amount to receive (so we dont receive any of the next frame)
	if (this->Stream.qwPayloadRemaining < dwBufferLength) {
		dwMaxReceive = (DWORD)this->Stream.qwPayloadRemaining;
	}
	else {
		dwMaxReceive = dwBufferLength;
	}

	// Receive payload
	errorCode = this->pHttpRequest->ReadEntityBody((CHAR*)pBuffer + *pdwBytesReceived, dwMaxReceive, FALSE, &dwBytesReceived, &fCompletionPending);
	if ((errorCode != S_OK) && (HRESULT_CODE(errorCode) != ERROR_MORE_DATA) && (HRESULT_CODE(errorCode) != ERROR_HANDLE_EOF)) {
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "ReadEntityBody() 'WebSocket payload'");
		goto exit;
	}

	// Reset error code because it could be ERROR_MORE_DATA or ERROR_HANDLE_EOF
	errorCode = S_OK;

	// Check if we should have received payload data
	if ((dwBytesReceived == 0) && (this->Stream.qwPayloadRemaining != 0)) {
		goto receiveLoop;
	}

	// Adjust payload remaining
	this->Stream.qwPayloadRemaining -= dwBytesReceived;

	// Set bytes received for the transfer
	*pdwBytesReceived += dwBytesReceived;

	// Are we done with this payload?
	if (this->Stream.qwPayloadRemaining != 0)
	{
		// This is a fragment because we haven't finished receiving the payload
		if (this->WebSocketFrame.Opcode == 0x01) {
			*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE;
		}
		else if (this->WebSocketFrame.Opcode == 0x02) {
			*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE;
		}
		else if ((this->WebSocketFrame.Opcode == 0x08) || (this->WebSocketFrame.Opcode == 0x09) || (this->WebSocketFrame.Opcode == 0x0A)) {
			// "Connection close", "Ping" and "Pong" must be received in a single buffer
			if (*pdwBytesReceived != dwBufferLength) {
				goto receiveLoop;
			}
			else {
				errorCode = ERROR_NOT_ENOUGH_MEMORY;
				PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::Receive() 'Connection close, Ping, Pong'");
				goto exit;
			}
		}

		// Could be a "Continuation frame", we assume `*pBufferType` has already been set
	}
	else
	{
		// This could be a fragment, or the end of the message
		if (this->WebSocketFrame.FIN)
		{
			// This is the final fragment in a message
			if (this->WebSocketFrame.Opcode == 0x00) {
				// "Continuation frame"
				if (*pBufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) {
					*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
				}
				else if (*pBufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE) {
					*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
				}
			}
			else if (this->WebSocketFrame.Opcode == 0x01) {
				// "Text frame"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x02) {
				// "Binary frame"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x08) {
				// "Connection close"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x09) {
				// "Ping"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PING_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x0A) {
				// "Pong"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PONG_BUFFER_TYPE;
			}
		}
		else
		{
			// This is a fragment of a message
			if (this->WebSocketFrame.Opcode == 0x00) {
				// "Continuation frame"
				// ... Do nothing!
			}
			else if (this->WebSocketFrame.Opcode == 0x01) {
				// "Text frame"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x02) {
				// "Binary frame"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x08) {
				// "Connection close"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x09) {
				// "Ping"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PING_BUFFER_TYPE;
			}
			else if (this->WebSocketFrame.Opcode == 0x0A) {
				// "Pong"
				*pBufferType = IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PONG_BUFFER_TYPE;
			}
		}

		// We need to get a new frame on the next call
		this->Stream.bQueuing = true;
	}

	// Unmask the payload data if necessary
	if (this->WebSocketFrame.bMask)
	{
		for (unsigned long long i = 0; i < *pdwBytesReceived; i++)
		{
			((CHAR*)pBuffer)[i] ^= this->WebSocketFrame.MaskingKey[this->Stream.mkI++ % 4];
		}
	}

exit:

	// Set class error code
	this->ErrorCode = errorCode;

	// Return error code
	return errorCode;
}

DWORD WebSocketServer::Send(IIS_WEB_SOCKET_BUFFER_TYPE bufferType, void* pBuffer, DWORD dwLength)
{
	DWORD errorCode;
	DWORD dwFrameLength;
	DWORD dwBufferSize;
	UCHAR* pFrameBuffer;
	DWORD dwBytesSent;
	BOOL fCompletionExpected;
	DWORD dwTotalBytesSent;
	HTTP_DATA_CHUNK dataChunk;

	// Set success
	errorCode = S_OK;

	// Clear the response
	pHttpResponse->Clear();

	// Determine the frame & buffer size in bytes
	if (dwLength <= 125) {
		dwFrameLength = 2;
	}
	else if (dwLength <= 65535) {
		dwFrameLength = 4;
	}
	else {
		dwFrameLength = 10;
	}
	dwBufferSize = dwFrameLength + dwLength;

	// Allocate memory for the frame & payload
	pFrameBuffer = (UCHAR*)malloc(dwBufferSize);
	if (pFrameBuffer == NULL) {
		errorCode = ERROR_NOT_ENOUGH_MEMORY;
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::Send()");
		goto exit;
	}

	// Set FIN and Opcode in the frame
	switch (bufferType)
	{
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
		if (this->IsFragment) {
			pFrameBuffer[0] = 0x80; // End of multi frame message
		}
		else {
			pFrameBuffer[0] = 0x81; // A single frame message
		}
		this->IsFragment = FALSE;
		break;
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
		if (this->IsFragment) {
			pFrameBuffer[0] = 0x00; // Continuation frame
		}
		else {
			pFrameBuffer[0] = 0x01; // Start of multi frame message
			this->IsFragment = TRUE;
		}
		break;
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE:
		if (this->IsFragment) {
			pFrameBuffer[0] = 0x80; // End of multi frame data
		}
		else {
			pFrameBuffer[0] = 0x82; // A single frame of data
		}
		this->IsFragment = FALSE;
		break;
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
		if (this->IsFragment) {
			pFrameBuffer[0] = 0x00; // Continuation frame
		}
		else {
			pFrameBuffer[0] = 0x02; // Start of multi frame data
			this->IsFragment = TRUE;
		}
		break;
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE:
		pFrameBuffer[0] = 0x88;
		break;
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PING_BUFFER_TYPE:
		pFrameBuffer[0] = 0x89;
		break;
	case IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PONG_BUFFER_TYPE:
		pFrameBuffer[0] = 0x8A;
		break;
	default:
		errorCode = ERROR_INVALID_PARAMETER;
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WebSocketServer::Send 'bufferType'");
		goto exit;
	}

	// Set the payload length
	if (dwLength <= 125)
	{
		if (dwBufferSize >= 2) {
			pFrameBuffer[1] = (UCHAR)dwLength;
		}
	}
	else if (dwLength <= 65535)
	{
		if (dwBufferSize >= 4) {
			pFrameBuffer[1] = 126;
			pFrameBuffer[2] = (dwLength >> 8) & 0xFF;
			pFrameBuffer[3] = dwLength & 0xFF;
		}
	}
	else
	{
		if (dwBufferSize >= 10) {
			pFrameBuffer[1] = 127;
			pFrameBuffer[2] = 0;
			pFrameBuffer[3] = 0;
			pFrameBuffer[4] = 0;
			pFrameBuffer[5] = 0;
			pFrameBuffer[6] = (dwLength >> 24) & 0xFF;
			pFrameBuffer[7] = (dwLength >> 16) & 0xFF;
			pFrameBuffer[8] = (dwLength >> 8) & 0xFF;
			pFrameBuffer[9] = dwLength & 0xFF;
		}
	}

	// Append the payload to the frame
	memcpy(pFrameBuffer + dwFrameLength, pBuffer, (size_t)dwLength);

	// Set parameters
	dwBytesSent = 0;
	fCompletionExpected = FALSE;
	dwTotalBytesSent = 0;

	// Write chunks until all data has been written
	while (dwTotalBytesSent < dwBufferSize)
	{
		// Setup chunk
		dataChunk.DataChunkType = HTTP_DATA_CHUNK_TYPE::HttpDataChunkFromMemory;
		dataChunk.FromMemory.pBuffer = pFrameBuffer + dwTotalBytesSent;
		dataChunk.FromMemory.BufferLength = dwBufferSize - dwTotalBytesSent;

		// Write chunk
		errorCode = pHttpResponse->WriteEntityChunks(&dataChunk, 1, FALSE, TRUE, &dwBytesSent, &fCompletionExpected);
		if (errorCode != S_OK) {
			PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "WriteEntityChunks()");
			goto exit;
		}

		// Increase total bytes sent
		dwTotalBytesSent += dwBytesSent;

		// Reset parameters
		dwBytesSent = 0;
		fCompletionExpected = FALSE;
	}

	// Flush response
	errorCode = pHttpResponse->Flush(FALSE, TRUE, &dwBytesSent, &fCompletionExpected);
	if (errorCode != S_OK) {
		PrintLastError(errorCode, this->ErrorDescription, this->ErrorBufferLength, "Flush()");
		goto exit;
	}

exit:

	// Free resources
	if (pFrameBuffer) {
		free(pFrameBuffer);
	}

	// Set class error code
	this->ErrorCode = errorCode;

	// Return error code
	return errorCode;
}

BOOL WebSocketServer::IsConnected()
{
	return this->pHttpConnection->IsConnected();
}

VOID WebSocketServer::Free()
{
	if (this->Stream.pFrameBuffer) {
		free(this->Stream.pFrameBuffer);
	}

	if (this->ErrorDescription) {
		free(this->ErrorDescription);
	}

	if (this->pRequestHeaders) {
		free(this->pRequestHeaders);
	}
}
