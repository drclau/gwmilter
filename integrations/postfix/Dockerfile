FROM alpine:3.21

RUN apk add --update postfix envsubst && \
    rm -rf /var/cache/apk/*

COPY --chown=root:root --chmod=755 integrations/postfix/entrypoint.sh /app/

EXPOSE 25 465 587

ENTRYPOINT ["/app/entrypoint.sh"]
