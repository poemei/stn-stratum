/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_chain_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_space(const char **p)
{
    while(**p!='\0' && isspace((unsigned char)**p)){
        (*p)++;
    }
}

static int parse_string(const char **p,char **out)
{
    const char *start;
    const char *scan;
    char *value;
    size_t length;
    size_t i;
    size_t j;

    skip_space(p);

    if(**p!='"'){return 0;}
    (*p)++;

    start=*p;
    scan=start;
    length=0u;

    while(*scan!='\0' && *scan!='"'){
        if(*scan=='\\'){
            scan++;
            if(*scan=='\0'){return 0;}
        }

        scan++;
        length++;
    }

    if(*scan!='"'){return 0;}

    value=(char *)malloc(length+1u);
    if(value==NULL){return 0;}

    i=0u;
    j=0u;

    while(start+i<scan){
        if(start[i]=='\\'){
            i++;

            switch(start[i]){
            case '"': value[j++]='"'; break;
            case '\\': value[j++]='\\'; break;
            case '/': value[j++]='/'; break;
            case 'b': value[j++]='\b'; break;
            case 'f': value[j++]='\f'; break;
            case 'n': value[j++]='\n'; break;
            case 'r': value[j++]='\r'; break;
            case 't': value[j++]='\t'; break;
            default:
                free(value);
                return 0;
            }

            i++;
            continue;
        }

        value[j++]=start[i++];
    }

    value[j]='\0';

    *p=scan+1;
    *out=value;
    return 1;
}

static int parse_port(const char **p,uint16_t *port)
{
    unsigned long value=0ul;
    int have_digit=0;

    skip_space(p);

    while(isdigit((unsigned char)**p)){
        unsigned digit=(unsigned)(**p-'0');

        have_digit=1;

        if(value>6553ul ||
           (value==6553ul && digit>5u))
        {
            return 0;
        }

        value=(value*10ul)+digit;
        (*p)++;
    }

    if(!have_digit || value==0ul){return 0;}

    *port=(uint16_t)value;
    return 1;
}

static int expect(const char **p,char value)
{
    skip_space(p);

    if(**p!=value){return 0;}

    (*p)++;
    return 1;
}

static int parse_server(
    const char **p,
    stn_chain_config_entry *entry)
{
    char *key=NULL;
    char *host=NULL;
    uint16_t port=0u;
    int have_host=0;
    int have_port=0;

    if(!expect(p,'{')){return 0;}

    for(;;){
        skip_space(p);

        if(**p=='}'){
            (*p)++;
            break;
        }

        if(!parse_string(p,&key)){goto fail;}
        if(!expect(p,':')){goto fail;}

        if(strcmp(key,"host")==0){
            if(have_host){goto fail;}

            if(!parse_string(p,&host)){goto fail;}

            if(host[0]=='\0'){goto fail;}

            have_host=1;
        }
        else if(strcmp(key,"port")==0){
            if(have_port){goto fail;}

            if(!parse_port(p,&port)){goto fail;}

            have_port=1;
        }
        else{
            goto fail;
        }

        free(key);
        key=NULL;

        skip_space(p);

        if(**p==','){
            (*p)++;
            continue;
        }

        if(**p=='}'){
            (*p)++;
            break;
        }

        goto fail;
    }

    if(!have_host || !have_port){goto fail;}

    entry->host=host;
    entry->port=port;
    return 1;

fail:
    free(key);
    free(host);
    return 0;
}

static int append_server(
    stn_chain_config *config,
    stn_chain_config_entry *entry)
{
    stn_chain_config_entry *servers;
    size_t count;

    if(config->server_count==SIZE_MAX /
       sizeof(*config->servers))
    {
        return 0;
    }

    count=config->server_count+1u;

    servers=(stn_chain_config_entry *)realloc(
        config->servers,
        count*sizeof(*config->servers));

    if(servers==NULL){return 0;}

    config->servers=servers;
    config->servers[config->server_count]=*entry;
    config->server_count=count;

    entry->host=NULL;
    entry->port=0u;

    return 1;
}

static int parse_servers(
    const char **p,
    stn_chain_config *config)
{
    if(!expect(p,'[')){return 0;}

    skip_space(p);

    if(**p==']'){
        (*p)++;
        return 0;
    }

    for(;;){
        stn_chain_config_entry entry;

        memset(&entry,0,sizeof(entry));

        if(!parse_server(p,&entry)){return 0;}

        if(!append_server(config,&entry)){
            free(entry.host);
            return 0;
        }

        skip_space(p);

        if(**p==','){
            (*p)++;
            continue;
        }

        if(**p==']'){
            (*p)++;
            return 1;
        }

        return 0;
    }
}

static int parse_config(
    const char *text,
    stn_chain_config *config)
{
    const char *p=text;
    char *key=NULL;
    int have_servers=0;

    if(!expect(&p,'{')){return 0;}

    for(;;){
        skip_space(&p);

        if(*p=='}'){
            p++;
            break;
        }

        if(!parse_string(&p,&key)){goto fail;}
        if(!expect(&p,':')){goto fail;}

        if(strcmp(key,"chain_servers")==0){
            if(have_servers){goto fail;}

            if(!parse_servers(&p,config)){goto fail;}

            have_servers=1;
        }
        else{
            goto fail;
        }

        free(key);
        key=NULL;

        skip_space(&p);

        if(*p==','){
            p++;
            continue;
        }

        if(*p=='}'){
            p++;
            break;
        }

        goto fail;
    }

    skip_space(&p);

    if(*p!='\0' || !have_servers ||
       config->server_count==0u)
    {
        goto fail;
    }

    return 1;

fail:
    free(key);
    return 0;
}

static char *read_file(const char *path)
{
    FILE *file;
    long length;
    char *text;
    size_t read_length;

#ifdef _MSC_VER
    if(fopen_s(&file,path,"rb")!=0){
        return NULL;
    }
#else
    file=fopen(path,"rb");
    if(file==NULL){
        return NULL;
    }
#endif

    if(fseek(file,0,SEEK_END)!=0){
        fclose(file);
        return NULL;
    }

    length=ftell(file);
    if(length<0){
        fclose(file);
        return NULL;
    }

    if(fseek(file,0,SEEK_SET)!=0){
        fclose(file);
        return NULL;
    }

    if((unsigned long)length>=SIZE_MAX){
        fclose(file);
        return NULL;
    }

    text=(char *)malloc((size_t)length+1u);
    if(text==NULL){
        fclose(file);
        return NULL;
    }

    read_length=fread(text,1u,(size_t)length,file);

    if(read_length!=(size_t)length){
        free(text);
        fclose(file);
        return NULL;
    }

    text[read_length]='\0';

    fclose(file);
    return text;
}

int stn_chain_config_load(
    stn_chain_config *config,
    const char *path)
{
    stn_chain_config loaded;
    char *text;

    if(config==NULL || path==NULL){return 0;}

    memset(&loaded,0,sizeof(loaded));

    text=read_file(path);
    if(text==NULL){return 0;}

    if(!parse_config(text,&loaded)){
        free(text);
        stn_chain_config_close(&loaded);
        return 0;
    }

    free(text);

    stn_chain_config_close(config);
    *config=loaded;

    return 1;
}

void stn_chain_config_close(
    stn_chain_config *config)
{
    size_t i;

    if(config==NULL){return;}

    for(i=0u;i<config->server_count;i++){
        free(config->servers[i].host);
    }

    free(config->servers);

    config->servers=NULL;
    config->server_count=0u;
}