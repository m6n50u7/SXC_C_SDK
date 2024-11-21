#include <pthread.h>
#include <vws/websocket.h>
typedef struct list list_member;
struct list
{
	list_member* previous;
	unsigned int CorrID;
	int dataType;
	void* receiver;
	list_member* next;
};
typedef struct group
{
	char *name;
	char *localName;
	char *link;
	char *pic;
	char *role;
	char *ID;
} group;
typedef struct contact
{
	char *name;
	char *localName;
	char *chatroomLocalName;
	char *link;
	char *role;
	char *pic;
	char *ID;
	int status;
} contact;
typedef struct msg
{
	char *senderName;
	char *senderLocalName;
	char *chatroomName;
	char *chatroomLocalName;
	char *reply_toText;
	char *reply_toImage;
	char *text;
	char *file;
	char *image;
	char *sendTime;
	char *editTime;
	char *ID;
	int status;
} msg;
typedef struct profile
{
	void (*OnMessage)(msg message);
	void (*OnConnection)(group chatroom);
	void (*OnGroupChange)(group chatroom);
	void (*OnContactChange)(contact cont);
	void (*OnGroupJoinRequest)(contact cont, group chatroom);
	void (*OnNewMember)(group chatroom, contact member);
	char *name;
	char *link;
	vws_cnx* connection;
	pthread_t thread;
	list_member* first;
	list_member* last;
	unsigned int list_members;
} profile;
#define SXCbot_SUCCESS 1;
#define SXCbot_CONNECTION_FAILED -1;
#define SXCbot_DESTINATION_NOT_FOUND -2;
#define SXCbot_NOT_AUTHORIZED -3;
#define SXCbot_EMPTY -4;
#define SXCbot_EOL 2;
#define SXCbot_DATA_TYPE_INTEGER 1;
#define SXCbot_DATA_TYPE_STRING 2;
#define SXCbot_DATA_TYPE_MESSAGE 3;
#define SXCbot_DATA_TYPE_GROUP 4;
#define SXCbot_DATA_TYPE_CONTACT 5;
int SXCbot_connect(profile* prof, char *connection, void (*OnMessage)(msg message), void (*OnConnection)(group chatroom), void (*OnGroupChange)(group chatroom), void (*OnContactChange)(contact cont), void (*OnGroupJoinRequest)(contact cont, group chatroom), void (*OnNewMember)(group chatroom, contact member));
void SXCbot_disconnect(profile* prof);
int SXCbot_sendMessage(profile* prof, contact target, char* message, char* file);
int SXCbot_sendGroupMessage(profile* prof, group target, char* message, char* file);
int SXCbot_joinChat(profile* prof, char *target);
int SXCbot_addContactToGroup(profile* prof, contact member, group chatroom, char* role);
int SXCbot_remContactFromGroup(profile* prof, contact member, group chatroom);
contact* SXCbot_getGroupMembers(profile* prof, group chatroom, unsigned int* N);
msg* SXCbot_getLastNMessages(profile* prof, unsigned int N);
msg* SXCbot_getLastNMessagesFromContact(profile* prof, contact target, unsigned int N);
msg* SXCbot_getLastNMessagesFromChat(profile* prof, group target, unsigned int N);
int SXCbot_deleteMessageToContact(profile* prof, contact target, char *startsWith);
int SXCbot_deleteMessageToGroup(profile* prof, group target, char *startsWith);
int SXCbot_editMessageToContact(profile* prof, contact target, char *startsWith, char *newText);
int SXCbot_editMessageToGroup(profile* prof, group target, char *startsWith, char *newText);
int SXCbot_replyToMessageToContact(profile* prof, contact target, char *startsWith, int yours, char *reply);
int SXCbot_replyToMessageToGroup(profile* prof, group target, char *startsWith, char *reply);
int SXCbot_replyToMessageToGroupFromContact(profile* prof, group target, char *startsWith, contact from, char *reply);
int SXCbot_forwardImageToContact(profile* prof, char *source, contact target);
int SXCbot_forwardImageToGroup(profile* prof, char *source, group target);
int SXCbot_forwardFileToContact(profile* prof, char *source, contact target);
int SXCbot_forwardFileToGroup(profile* prof, char *source, group target);
int SXCbot_receiveFile(profile* prof, char *source, char *destination);
