# Start with the express-c development image
FROM ghcr.io/williamcotton/express-c:master AS buildstage

ENV LD_LIBRARY_PATH /usr/local/lib

WORKDIR /app

COPY . .

RUN cp -Rp /usr/local/include/src/* /usr/local/include
RUN cp -Rp /usr/local/include/deps/* /usr/local/include
RUN ls -al /usr/local/include

RUN ls -al /app/public
RUN ls -al /app/views

# Build the production app
RUN make TodoMVC-prod

RUN cat /app/embeddedFiles.h

# Copy the shared objects to the /app/sdeps directory
RUN ldd /app/build/TodoMVC | tr -s '[:blank:]' '\n' | grep '^/' | \
  xargs -I % sh -c 'mkdir -p $(dirname sdeps%); cp % sdeps%;'

# Download dbmate
RUN curl -fsSL -o /usr/local/bin/dbmate https://github.com/amacneil/dbmate/releases/latest/download/dbmate-linux-amd64
RUN chmod +x /usr/local/bin/dbmate

# Grab a fresh copy of our linux image
FROM ubuntu:hirsute AS deploystage

WORKDIR /app

# Copy the shared objects to our new image
COPY --from=buildstage /app/sdeps/lib /usr/lib
COPY --from=buildstage /app/sdeps/usr/local/lib /usr/lib
RUN ls -al /usr/lib

# Copy binaries to the new image
COPY --from=buildstage /usr/local/bin /usr/local/bin
RUN ls -al /usr/local/bin

# Copy the app executable
COPY --from=buildstage /app/build/TodoMVC /app/build/TodoMVC
RUN ls -al /app/build

# Copy our database migrations
COPY --from=buildstage /app/db/migrations /app/db/migrations
RUN ls -al /app/db/migrations

RUN ls -al /app

# Run the app
CMD /app/build/TodoMVC
