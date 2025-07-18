// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define USING_SHARED_PTR(name) using name##Ref = TSharedPtr<class name>;

USING_SHARED_PTR(SendBuffer);
USING_SHARED_PTR(Session);
USING_SHARED_PTR(PacketSession);

#define SEND_PACKET(Pkt)														\
	Cast<UMyGameInstance>(GWorld->GetGameInstance())->SendPacket(Pkt);	

#define GET_SESSION()															\
	Cast<UMyGameInstance>(GWorld->GetGameInstance())->mySession;

#define BYTE uint8

class FSocket;

	enum EMySocketType
	{
		Server,
		Client,
	};
	enum EPacketType
	{
		Lockstep,
		Snapshot,
		Sync,
		Ack,
	};

	struct SERVERP1_API FPacketHeader
	{
		FPacketHeader() : PacketSize(0), PacketID(0)
		{

		}

		FPacketHeader(uint16 PacketSize, uint16 PacketID) : PacketSize(PacketSize), PacketID(PacketID)
		{

		}

		friend FArchive& operator<<(FArchive& Ar, FPacketHeader& Header) {
			Ar << Header.PacketSize;
			Ar << Header.PacketID;
			return Ar;
		}

		uint16 PacketSize;
		uint16 PacketID;
	};

	struct  FMyInput
	{
		bool left;
		bool right;
		bool up;
		bool down;
		bool jump;
	};


	struct  FMyState
	{
		FVector position;
    FQuat rotation;
		FVector velocity;
	};

	struct IPacket
	{
		virtual void Serialize(uint8* ptr) { }
		virtual void Deserialize(uint8* ptr) { }
		virtual uint16 ByteSize() { return 0; }
	};

	struct FLockstepPacket : public IPacket
	{
		uint16 nSeq;
		FMyInput input;

		virtual void Serialize(uint8* ptr)
		{
      FLockstepPacket* packet = reinterpret_cast<FLockstepPacket*>(ptr);
      packet->nSeq = nSeq;
      packet->input = input;
		}

		virtual void Deserialize(uint8* ptr)
		{
      FLockstepPacket* packet = reinterpret_cast<FLockstepPacket*>(ptr);
      nSeq = packet->nSeq;
      input = packet->input;
		}

		virtual uint16 ByteSize() 
		{ 
			return sizeof(FMyInput) + sizeof(nSeq);
		}
	};

	struct FSyncPacket : public IPacket
	{
		uint16 nSeq;
		uint16 nInputSize;
		TArray<FMyInput> inputs;
		uint16 nStateSize;
		TArray<FMyState> states;

    virtual void Serialize(uint8* ptr)
       {
           uint8* cursor = ptr;

           // Sequence number
           *reinterpret_cast<uint16*>(cursor) = nSeq;
           cursor += sizeof(uint16);

           // Input count
           *reinterpret_cast<uint16*>(cursor) = nInputSize;
           cursor += sizeof(uint16);

           // Inputs
           for (const FMyInput& input : inputs)
           {
               FMemory::Memcpy(cursor, &input, sizeof(FMyInput));
               cursor += sizeof(FMyInput);
           }

           // State count
           *reinterpret_cast<uint16*>(cursor) = nStateSize;
           cursor += sizeof(uint16);

           // States
           for (const FMyState& state : states)
           {
               FMemory::Memcpy(cursor, &state, sizeof(FMyState));
               cursor += sizeof(FMyState);
           }
       }

       // Deserialize a packet from a binary buffer
       virtual void Deserialize(uint8* ptr)
       {
           const uint8* cursor = ptr;

           // Sequence number
           nSeq = *reinterpret_cast<const uint16*>(cursor);
           cursor += sizeof(uint16);

           // Input count
           nInputSize = *reinterpret_cast<const uint16*>(cursor);
           cursor += sizeof(uint16);

           // Inputs
           inputs.SetNum(nInputSize);
           for (int i = 0; i < nInputSize; ++i)
           {
               FMemory::Memcpy(&inputs[i], cursor, sizeof(FMyInput));
               cursor += sizeof(FMyInput);
           }

           // State count
           nStateSize = *reinterpret_cast<const uint16*>(cursor);
           cursor += sizeof(uint16);

           // States
           states.SetNum(nStateSize);
           for (int i = 0; i < nStateSize; ++i)
           {
               FMemory::Memcpy(&states[i], cursor, sizeof(FMyState));
               cursor += sizeof(FMyState);
           }
       }


		virtual uint16 ByteSize()
		{
			return sizeof(FMyInput) + sizeof(nInputSize) + 
				sizeof(FMyInput) * nInputSize + 
				sizeof(nStateSize) + sizeof(FMyState) * nStateSize ; 
		}
	};

	struct FSnapshotPacket : public IPacket
	{
		uint16 nSeq;
		uint16 nStateSize;
		TArray<FMyState> states;

    virtual void Serialize(uint8* ptr)
      {
        uint8* cursor = ptr;

        // Serialize sequence number
        *reinterpret_cast<uint16*>(cursor) = nSeq;
        cursor += sizeof(uint16);

        // Serialize number of states
        *reinterpret_cast<uint16*>(cursor) = nStateSize;
        cursor += sizeof(uint16);

        // Serialize each state
        for (const FMyState& state : states)
        {
          FMemory::Memcpy(cursor, &state, sizeof(FMyState));
          cursor += sizeof(FMyState);
        }
      }

      virtual void Deserialize(uint8* ptr)
      {
        const uint8* cursor = ptr;

        // Read sequence number
        nSeq = *reinterpret_cast<const uint16*>(cursor);
        cursor += sizeof(uint16);

        // Read number of states
        nStateSize = *reinterpret_cast<const uint16*>(cursor);
        cursor += sizeof(uint16);

        // Read states
        states.SetNum(nStateSize);
        for (int i = 0; i < nStateSize; ++i)
        {
          FMemory::Memcpy(&states[i], cursor, sizeof(FMyState));
          cursor += sizeof(FMyState);
        }
      }


		virtual uint16 ByteSize()
		{
			return sizeof(FMyInput) +
				sizeof(nStateSize) + sizeof(FMyState) * nStateSize;
		}
	};

	struct FAckPacket : public IPacket
	{
		uint16 nAck;

		virtual void Serialize(uint8* ptr)
		{
			FAckPacket* packet = reinterpret_cast<FAckPacket*>(ptr);
			packet->nAck = nAck;
		}

		virtual void Deserialize(uint8* ptr)
		{
		}

		virtual uint16 ByteSize()
		{
			return sizeof(nAck);
		}
	};

	struct FSessionState
	{
		int64 ack;
		int64 seq;
	};

class SendBuffer : public TSharedFromThis<SendBuffer>	// �ٷιٷ� ����
{
public:
	SendBuffer(int32 bufferSize);
	~SendBuffer();

	BYTE* Buffer() { return _buffer.GetData(); }
	int32 WriteSize() { return _writeSize; }
	int32 Capacity() { return static_cast<int32>(_buffer.Num()); }

	void CopyData(void* data, int32 len);
	void Close(uint32 writeSize);

private:
	TArray<BYTE> _buffer;
	int32 _writeSize = 0;
};

class SERVERP1_API RecvWorker : public FRunnable
{
public:
	RecvWorker(FSocket* Socket, TSharedPtr<class PacketSession> Session, TSharedRef<FInternetAddr> RemoteAddr);
	~RecvWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;

	void Destroy();

private:
	bool ReceivePacket(TArray<uint8>& OutPacket);
	bool ReceiveDesiredBytes(uint8* Results, int32 Size);

protected:
	FRunnableThread* Thread = nullptr;
	bool Running = true;
	FSocket* Socket;
	TWeakPtr<class PacketSession> SessionRef;
	TSharedRef<FInternetAddr> RemoteAddr;
};

class SERVERP1_API SendWorker : public FRunnable
{
public:
	SendWorker(FSocket* Socket, TSharedPtr<class PacketSession> Session, TSharedRef<FInternetAddr> RemoteAddr);
	~SendWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;

	bool SendPacket(SendBufferRef SendBuffer);
	void Destroy();

private:
	bool SendDesiredBytes(const uint8* Buffer, int32 Size);

protected:
	FRunnableThread* Thread = nullptr;
	bool Running = true;
	FSocket* Socket;
	TWeakPtr<class PacketSession> SessionRef;
	TSharedRef<FInternetAddr> RemoteAddr;
};

class SERVERP1_API PacketSession : public TSharedFromThis<PacketSession>
{
public:
	PacketSession(FSocket* Socket, bool bServer);
	~PacketSession();

	void Run();
	void Recv();

	void HandleRecvPackets();	// Delayed Out Buffer

	void HandlePacket(TArray<uint8>& Packet);
	void HandleServerPacket(TArray<uint8>& Packet);
	void HandleClientPacket(TArray<uint8>& Packet);

	void SendPacket(SendBufferRef SendBuffer);
	
	template<typename T>
	static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
	{
		const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		const uint16 packetSize = dataSize + sizeof(FPacketHeader);

		SendBufferRef sendBuffer = MakeShared<SendBuffer>(packetSize);

		FPacketHeader* header = reinterpret_cast<FPacketHeader*>(sendBuffer->Buffer());
		header->PacketSize = packetSize;
		header->PacketID = pktId;

		switch (pktId)
		{
		case EPacketType::Lockstep:
		{

		}
			break;

		case EPacketType::Snapshot:
		{

		}
			break;

		case EPacketType::Sync:	
		{

		}
			break;
			
		case EPacketType::Ack:
		{
			
		}
			break;
		}
		
		pkt.SerializeToArray(&header[1], dataSize);
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}

public:
	FSocket* Socket;

	TSharedPtr<class RecvWorker> RecvWorkerThread;
	TSharedPtr<class SendWorker> SendWorkerThread;

	TQueue<TArray<uint8>> RecvPacketQueue;
	TQueue<SendBufferRef> SendPacketQueue;

	bool bServer;
};
