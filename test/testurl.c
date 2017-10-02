#include <stdio.h>
#include <string.h>
#include "api.h"

int main()
{
    char src[] = "http://example.com:80/a/dir/test.wav";
    char dst[] = "http://www.another-example.com/file.mp3";
    char *url;

    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "http://www.another-example.com/file.mp3")) {
        printf("%s does not match\nhttp://www.another-example.com/file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "http://example.com:10/a/dir/test.wav");
    sprintf(dst, "%s", "http://www.another-example.com/file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "http://www.another-example.com/file.mp3")) {
        printf("%s does not match\nhttp://www.another-example.com/file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "http://example.com:10/a/dir/test.wav");
    sprintf(dst, "%s", "../../file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "http://example.com:10/a/dir/../../file.mp3")) {
        printf("%s does not match\nhttp://example.com:10/a/dir/../../file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "example.com:7777/test/dir/test.wav");
    sprintf(dst, "%s", "www.another-example.com//file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "www.another-example.com//file.mp3")) {
        printf("%s does not match\nwww.another-example.com//file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "example.com/test/dir/test.wav");
    sprintf(dst, "%s", "../../file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "example.com/test/dir/../../file.mp3")) {
        printf("%s does not match\nexample.com/test/dir/../../file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "http://example.com");
    sprintf(dst, "%s", "file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "http://example.com/file.mp3")) {
        printf("%s does not match\nhttp://example.com/file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "/tmp/test/directory/test.wav");
    sprintf(dst, "%s", "../../file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "/tmp/test/directory/../../file.mp3")) {
        printf("%s does not match\n/tmp/test/directory/../../file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "/tmp/test/directory/test.wav");
    sprintf(dst, "%s", "/file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "/file.mp3")) {
        printf("%s does not match\n/file.mp3\n", url);
        exit(-1);
    }

    sprintf(src, "%s", "/");
    sprintf(dst, "%s", "file.mp3");
    url = _aaxURLConstruct(src, dst);
    if (strcmp(url, "/file.mp3")) {
        printf("%s does not match\n/file.mp3\n", url);
        exit(-1);
    }

    return 0;
}
