-- migrate:up
CREATE TABLE todos (
  id SERIAL PRIMARY KEY,
  title text NOT NULL,
  completed boolean NOT NULL DEFAULT false
);
-- migrate:down
DROP TABLE todos;