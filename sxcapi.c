#include <stdio.h>
#include <stdlib.h>
#include <json.h>
#include <mkjson.h>
#include <time.h>
#include "sxcapi.h"
static void add_list_member(profile* prof, list_member neo)
{
	list_member *item = malloc(sizeof(list_member));
	if(prof->list_members == 0)
		prof->first = item;
	*item = neo;
	item->previous = prof->last;
	item->next = prof->first;
	prof->last = item;
	prof->list_members++;
}
static void submit_CorrID(profile* prof, unsigned int CorrID, void* receiver, int dataType)
{
	list_member sub;
	sub.CorrID = CorrID;
	sub.receiver = receiver;
	sub.dataType = dataType;
	add_list_member(prof, sub);
}
static int power(int x, int y)
{
	int j=1;
	for(int i=0;i < y ;i++)
		j = j * x;
	return j;
}
static char int2char(int intgr)
{
	if(intgr == 0)
		return '0';
	if(intgr == 1)
		return '1';
	if(intgr == 2)
		return '2';
	if(intgr == 3)
		return '3';
	if(intgr == 4)
		return '4';
	if(intgr == 5)
		return '5';
	if(intgr == 6)
		return '6';
	if(intgr == 7)
		return '7';
	if(intgr == 8)
		return '8';
	if(intgr == 9)
		return '9';
	return '\0';
}
static int char2int(char chr)
{
	if(chr == '0')
		return 0;
	if(chr == '1')
		return 1;
	if(chr == '2')
		return 2;
	if(chr == '3')
		return 3;
	if(chr == '4')
		return 4;
	if(chr == '5')
		return 5;
	if(chr == '6')
		return 6;
	if(chr == '7')
		return 7;
	if(chr == '8')
		return 8;
	if(chr == '9')
		return 9;
	return 0;
}
static void int2str(int intgr, char str[])
{
	int pow;
	for(pow = 0; intgr > power(10, pow) - 1; pow++){}
	for(int i = 1;i < pow + 1; i++)
	{
		str[i - 1] = int2char(intgr / power(10, pow - i) - ((intgr / power(10, pow + 1 - i)) * 10));
		str[i] = '\0';
	}
}
static void send_command(profile* prof, unsigned int CorrID, char *command)
{
	char corri[13];
	char corrid[15] = "id";
	int2str(CorrID, corri);
	strcat(corrid, corri);
	char *json = mkjson(MKJSON_OBJ, 2, MKJSON_STRING, "cmd", command, MKJSON_STRING, "corrId", corrid);
	vws_msg_send_text(prof->connection, json);
	free(json);
}
static list_member get_CorrID_related_member(profile* prof, unsigned int CorrID)
{
	list_member ret;
	list_member* index = prof->first;
	for(int i = 0; i < prof->list_members; i++)
	{
		if(CorrID == index->CorrID)
		{
			ret = *index;
			list_member* next = (list_member*) index->next;
			list_member* previous = (list_member*) index->previous;
			next->previous = previous;
			previous->next = next;
			if(i == 0)
				prof->first = next;
			prof->list_members--;
			free(index);
			return ret;
		}
		index = index->next;
	}
	return ret;
}
static void free_all(list_member* first, list_member* last)
{
	list_member *index = first, *next;
	while(index != last)
	{
		next = index->next;
		free(index);
		index = next;
	}
	free(last);
}
static unsigned int gen_CorrID()
{
	return (rand() % 999999) + 1;
}
static char* get_json_string(char *jsonString, char *jsonKey)
{
	struct json value = json_get(jsonString, jsonKey);
	size_t size = json_string_length(value);
	char *string = malloc(size + 1);
	json_string_copy(value, string, size);
	return string;
}
static int64_t get_json_int(char *jsonString, char *jsonKey)
{
	return json_int64(json_get(jsonString, jsonKey));
}
static void* event_checker(void* arg) 
{
	profile *prof;
	prof = (profile*) arg;
	assert(vws_socket_is_connected((vws_socket*)prof->connection) == true);
	while(1)
	{
		vws_msg* reply = vws_msg_recv(prof->connection);
		if (reply == NULL)
		{
			// There was no message received and it resulted in timeout
		}
		else
		{
			char *json = reply->data->data;
			if(json_exists(json_get(json, "corrId")))
			{
				char *corrid = get_json_string(json, "corrId");
				unsigned int CorrID = 0;
				for(int i = 2; corrid[i]; i++)
				{
					CorrID *= 10;
					CorrID += char2int(corrid[i]);
				}
				list_member member = get_CorrID_related_member(prof, CorrID);
				char *type = get_json_string(json, "resp.type");
				if(member.dataType == SXCbot_DATA_TYPE_INTEGER)
				{
					int *sender = (int*) member.receiver;
					if(strcmp(type, "chatCmdError"))
					{
						char *errType = get_json_string(json, "resp.chatError.storeError.type");
						if(strstr(errType, "NotFound") != NULL)
						{
							*sender = SXCbot_DESTINATION_NOT_FOUND;
						}
						else
						{
							*sender = SXCbot_NOT_AUTHORIZED;
						}
						free(errType);
					}
					free(type);
					*sender = SXCbot_SUCCESS;
				}
				if(member.dataType == SXCbot_DATA_TYPE_MESSAGE)
				{
					msg **sender = (msg*) member.receiver;
					if(strcmp(type, "chatCmdError"))
					{
						sender[0] = malloc(sizeof(msg));
						sender[0]->status = SXCbot_DESTINATION_NOT_FOUND;
					}
					else
					{
						struct json items, index;
						items = json_get(json, "resp.chatItems");
						index = json_first(items);
						if(json_exists(index))
						{
							int i, size;
							for(size = 1; json_exists(index); index = json_next(index))
							{
								size++;
							}
							index = json_first(items);
							char *obj = json_raw(index);
							sender[0] = malloc(size * sizeof(msg));
							for(i = 0; i < size; i++)
							{
								char *type = get_json_string(obj, "chatItem.content.msgContent.type");
								sender[0][i].text = get_json_string(obj, "chatItem.content.msgContent.text");
								sender[0][i].ID = malloc(10);
								int2str(json_int64(json_get(obj, "chatItem.meta.ItemId")), ID);
								sender[0][i].senderLocalName = get_json_string(obj, "chatInfo.contact.localDisplayName");
								sender[0][i].senderName = get_json_string(obj, "chatInfo.profile.displayName");
								sender[0][i].sendTime = get_json_string(obj, "chatItem.meta.createdAt");
								sender[0][i].editTime = get_json_string(obj, "chatItem.meta.updatedAt");
								if(json_exists(json_get(obj, "chatItem.content.msgContent.image")))
								{
									sender[0][i].image = get_json_string(obj, "chatItem.content.msgContent.image");
								}
								if(json_exists(json_get(obj, "chatItem.file")))
								{
									sender[0][i].file = get_json_string(obj, "chatItem.file.fileName");
								}
								if(json_exists(json_get(obj, "chatInfo.groupInfo")))
								{
									sender[0][i].chatroomLocalName = get_json_string(obj, "chatInfo.groupInfo.localDisplayName");
									sender[0][i].chatroomName = get_json_string(obj, "chatInfo.groupInfo.groupProfile.displayName");
								}
								if(json_exists(json_get(obj, "chatItem.quotedItem")))
								{
									sender[0][i].reply_toText = get_json_string(obj, "chatItem.quotedItem.content.text");
									if(json_exists(json_get(obj, "chatItem.quotedItem.content.image")))
										sender[0][i].reply_toImage = get_json_string(obj, "chatItem.quotedItem.content.image");
								}
								if(i < (size - 1))
								{
									sender[i]->status = SXCbot_SUCCESS;
								}
								else
								{
									sender[i]->status = SXCbot_EOL;
								}
								index = json_next(index);
							}
						}
						else
						{
							sender[0] = malloc(sizeof(msg));
							sender[0]->status = SXCbot_EMPTY;
						}
					}
					free(type);
					sender[0]->status = SXCbot_SUCCESS;
				}
				if(member.dataType == SXCbot_DATA_TYPE_CONTACT)
				{//TODO: study the API reply and change the code accordingly
					msg *sender = (contact*) member.receiver;
					if(strcmp(type, "chatCmdError"))
					{
						sender[0] = malloc(sizeof(contact));
						sender[0]->status = SXCbot_DESTINATION_NOT_FOUND;
					}
					free(type);
					sender[0]->status = SXCbot_SUCCESS;
				}
				if(member.dataType == SXCbot_DATA_TYPE_GROUP)
				{//TODO: study the API reply and change the code accordingly
					msg *sender = (group*) member.receiver;
					if(strcmp(type, "chatCmdError"))
					{
						char *errType = get_json_string(json, "resp.chatError.storeError.type");
						if(strstr(errType, "NotFound") != NULL)
						{
							sender[0] = malloc(sizeof(contact));
							sender[0]->status = SXCbot_DESTINATION_NOT_FOUND;
						}
						else
						{
							*sender = SXCbot_NOT_AUTHORIZED;
						}
						free(errType);
					}
					free(type);
					*member.receiver = SXCbot_SUCCESS;
				}
			}
			else
			{
				
			}
			// Free message
			vws_msg_free(reply);
		}
	}
	return NULL;
}
int SXCbot_connect(profile *prof, char *connection, void (*OnMessage)(msg message), void (*OnConnection)(group chatroom), void (*OnGroupChange)(group chatroom), void (*OnContactChange)(contact cont), void (*OnGroupJoinRequest)(contact cont, group chatroom), void (*OnNewMember)(group chatroom, contact member))
{
	prof->connection = vws_cnx_new();
	prof->list_members = 0;
	srand(time(NULL));
	if (vws_connect(prof->connection, connection) == false)
	{
		printf("Failed to connect to the WebSocket server\n");
		vws_cnx_free(prof->connection);
		return SXCbot_FAILED;
	}
	prof->OnMessage = OnMessage;
	prof->OnConnection = OnConnection;
	prof->OnGroupChange = OnGroupChange;
	prof->OnContactChange = OnContactChange;
	prof->OnGroupJoinRequest = OnGroupJoinRequest;
	prof->OnNewMember = OnNewMember;
	pthread_create(&prof->thread, NULL, event_checker, prof);
	return SXCbot_SUCCESS;
}
void SXCbot_disconnect(profile* prof)
{
	vws_disconnect(prof->connection);
	vws_cnx_free(prof->connection);
	pthread_cancel(prof->thread);
	free_all(prof->first, prof->last);
}
int SXCbot_sendMessage(profile* prof, contact target, char* message, char* file)
{
	unsigned int CorrID = gen_CorrID();
	int success;
	char* command;
	if(file == NULL)
	{
		command = malloc(strlen(message) + strlen(target.localName) + 5);
		command[0] = '\0';
		strcat(command, "@'");
		strcat(command, target.localName);
		strcat(command, "' ");
		strcat(command, message);
	}
	else
	{
		command = malloc(strlen(file) + strlen(target.localName) + 10);
		command[0] = '\0';
		strcat(command, "/file @'");
		strcat(command, target.localName);
		strcat(command, "' ");
		strcat(command, file);
	}
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_sendGroupMessage(profile* prof, group target, char* message, char* file)
{
	unsigned int CorrID = gen_CorrID();
	int success;
	char* command;
	if(file == NULL)
	{
		command = malloc(strlen(message) + strlen(target.localName) + 5);
		command[0] = '\0';
		strcat(command, "#'");
		strcat(command, target.localName);
		strcat(command, "' ");
		strcat(command, message);
	}
	else
	{
		command = malloc(strlen(file) + strlen(target.localName) + 10);
		command[0] = '\0';
		strcat(command, "/file #'");
		strcat(command, target.localName);
		strcat(command, "' ");
		strcat(command, file);
	}
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_joinChat(profile* prof, char *target)
{
	unsigned int CorrID = gen_CorrID();
	int success;
	char* command;
	command = malloc(strlen(target) + 12);
	command[0] = '\0';
	strcat(command, "/connect '");
	strcat(command, target);
	strcat(command, "'");
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_addContactToGroup(profile* prof, contact member, group chatroom, char* role)
{
	unsigned int CorrID = gen_CorrID();
	int success;
	char* command;
	command = malloc(strlen(chatroom.localName) + strlen(member.localName) + strlen(role) + 12);
	command[0] = '\0';
	strcat(command, "/add '");
	strcat(command, chatroom.localName);
	strcat(command, "' '");
	strcat(command, member.localName);
	strcat(command, "' ");
	strcat(command, role);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_remContactFromGroup(profile* prof, contact member, group chatroom)
{
	unsigned int CorrID = gen_CorrID();
	int success;
	char* command;
	command = malloc(strlen(chatroom.localName) + strlen(member.localName) + 15);
	command[0] = '\0';
	strcat(command, "/remove '");
	strcat(command, chatroom.localName);
	strcat(command, "' '");
	strcat(command, member.localName);
	strcat(command, "'");
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
contact* SXCbot_getGroupMembers(profile* prof, group chatroom, unsigned int* N)
{
	unsigned int CorrID = gen_CorrID();
	contact* members = NULL;
	unsigned int n;
	char* command;
	command = malloc(strlen(chatroom.localName) + 11);
	command[0] = '\0';
	strcat(command, "/members '");
	strcat(command, chatroom.localName);
	strcat(command, "'");
	submit_CorrID(prof, CorrID, &members, SXCbot_DATA_TYPE_CONTACT);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(members == NULL)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	if(members[0].status == SXCbot_DESTINATION_NOT_FOUND)
	{
		*N = 0;
		free(command);
		return members;
	}
	for(n = 0; members[n] == SXCbot_SUCCESS; n++)
	{
		members[n].chatroomLocalName = chatroom.localName;
	}
	members[n].chatroomLocalName = chatroom.localName;
	*N = n + 1;
	free(command);
	return members;
}
msg* SXCbot_getLastNMessages(profile* prof, unsigned int N)
{
	unsigned int CorrID = gen_CorrID();
	msg* messages = NULL;
	char number[7];
	int2str(N, number);
	char* command;
	command = malloc(strlen(number) + 8);
	command[0] = '\0';
	strcat(command, "/tail ");
	strcat(command, number);
	submit_CorrID(prof, CorrID, &messages, SXCbot_DATA_TYPE_MESSAGE);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(messages == NULL)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return messages;
}
msg* SXCbot_getLastNMessagesFromContact(profile* prof, contact target, unsigned int N)
{
	unsigned int CorrID = gen_CorrID();
	msg* messages = NULL;
	char number[7];
	int2str(N, number);
	char* command;
	command = malloc(strlen(number) + strlen(target.localName) + 11);
	command[0] = '\0';
	strcat(command, "/tail @'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, number);
	submit_CorrID(prof, CorrID, &messages, SXCbot_DATA_TYPE_MESSAGE);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(messages == NULL)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return messages;
}
msg* SXCbot_getLastNMessagesFromChat(profile* prof, group target, unsigned int N)
{
	unsigned int CorrID = gen_CorrID();
	msg* messages = NULL;
	char number[7];
	int2str(N, number);
	char* command;
	command = malloc(strlen(number) + strlen(target.localName) + 11);
	command[0] = '\0';
	strcat(command, "/tail #'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, number);
	submit_CorrID(prof, CorrID, &messages, SXCbot_DATA_TYPE_MESSAGE);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(messages == NULL)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return messages;
}
int SXCbot_deleteMessageToContact(profile* prof, contact target, char *startsWith)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + 7);
	command[0] = '\0';
	strcat(command, "/ @'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, startsWith);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_deleteMessageToGroup(profile* prof, group target, char *startsWith)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + 7);
	command[0] = '\0';
	strcat(command, "/ #'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, startsWith);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_editMessageToContact(profile* prof, contact target, char *startsWith, char *newText)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + strlen(newText) + 10);
	command[0] = '\0';
	strcat(command, "! @'");
	strcat(command, target.localName);
	strcat(command, "' (");
	strcat(command, startsWith);
	strcat(command, ") ");
	strcat(command, newText);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_editMessageToGroup(profile* prof, group target, char *startsWith, char *newText)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + strlen(newText) + 10);
	command[0] = '\0';
	strcat(command, "! #'");
	strcat(command, target.localName);
	strcat(command, "' (");
	strcat(command, startsWith);
	strcat(command, ") ");
	strcat(command, newText);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_replyToMessageToContact(profile* prof, contact target, char *startsWith, int yours, char *reply)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + strlen(reply) + 11);
	command[0] = '\0';
	if(yours)
		strcat(command, ">> @'");
	else
		strcat(command, "> @'");
	strcat(command, target.localName);
	strcat(command, "' (");
	strcat(command, startsWith);
	strcat(command, ") ");
	strcat(command, reply);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_replyToMessageToGroup(profile* prof, group target, char *startsWith, char *reply)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + strlen(reply) + 10);
	command[0] = '\0';
	strcat(command, "> #'");
	strcat(command, target.localName);
	strcat(command, "' (");
	strcat(command, startsWith);
	strcat(command, ") ");
	strcat(command, reply);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_replyToMessageToGroupFromContact(profile* prof, group target, char *startsWith, contact from, char *reply)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(startsWith) + strlen(target.localName) + strlen(reply) + strlen(from.localName) + 14);
	command[0] = '\0';
	strcat(command, "> #'");
	strcat(command, target.localName);
	strcat(command, "' @'");
	strcat(command, from.localName);
	strcat(command, "' (");
	strcat(command, startsWith);
	strcat(command, ") ");
	strcat(command, reply);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_forwardImageToContact(profile* prof, char *source, contact target)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(source) + strlen(target.localName) + 11);
	command[0] = '\0';
	strcat(command, "/imgf @'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, source);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_forwardImageToGroup(profile* prof, char *source, group target)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(source) + strlen(target.localName) + 11);
	command[0] = '\0';
	strcat(command, "/imgf #'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, source);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_forwardFileToContact(profile* prof, char *source, contact target)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(source) + strlen(target.localName) + 15);
	command[0] = '\0';
	strcat(command, "/fforward @'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, source);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_forwardFileToGroup(profile* prof, char *source, group target)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(source) + strlen(target.localName) + 15);
	command[0] = '\0';
	strcat(command, "/fforward #'");
	strcat(command, target.localName);
	strcat(command, "' ");
	strcat(command, source);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
int SXCbot_receiveFile(profile* prof, char *source, char *destination)
{
	unsigned int CorrID = gen_CorrID();
	int success = 0;
	char* command;
	command = malloc(strlen(source) + strlen(destination) + 12);
	command[0] = '\0';
	strcat(command, "/freceive ");
	strcat(command, source);
	strcat(command, " ");
	strcat(command, destination);
	submit_CorrID(prof, CorrID, &success, SXCbot_DATA_TYPE_INTEGER);
	send_command(prof, CorrID, command);
	long long unsigned int nswaiting = 1000;
	while(!success)
	{
		struct timespec ts;
		ts.tv_sec = nswaiting / 1000000;
		ts.tv_nsec = nswaiting % 1000000;
		nanosleep(&ts, NULL);
		nswaiting *= 2;
	}
	free(command);
	return success;
}
