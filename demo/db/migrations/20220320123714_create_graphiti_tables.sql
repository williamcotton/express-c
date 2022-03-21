-- migrate:up
CREATE TABLE departments (
  id SERIAL PRIMARY KEY,
  name varchar(255) NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now()
);
CREATE TABLE teams (
  id SERIAL PRIMARY KEY,
  department_id integer NOT NULL,
  name varchar(255) NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now(),
  FOREIGN KEY (department_id) REFERENCES departments(id)
);
CREATE TABLE employees (
  id SERIAL PRIMARY KEY,
  first_name varchar(255) NOT NULL,
  last_name varchar(255) NOT NULL,
  age integer NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now()
);
CREATE TABLE milestones (
  id SERIAL PRIMARY KEY,
  epic_id integer NOT NULL,
  name varchar(255) NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now()
);
CREATE TABLE notes (
  id SERIAL PRIMARY KEY,
  notable_type varchar(255) NOT NULL,
  notable_id integer NOT NULL,
  body text NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now()
);
CREATE TABLE positions (
  id SERIAL PRIMARY KEY,
  employee_id integer NOT NULL,
  title varchar(255) NOT NULL,
  historical_index integer NOT NULL,
  active boolean NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now(),
  department_id integer NOT NULL,
  FOREIGN KEY (department_id) REFERENCES departments(id)
  FOREIGN KEY (employee_id) REFERENCES employees(id)
);
CREATE TABLE tasks (
  id SERIAL PRIMARY KEY,
  employee_id integer NOT NULL,
  team_id integer NOT NULL,
  type varchar(255) NOT NULL,
  title varchar(255) NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now(),
  FOREIGN KEY (employee_id) REFERENCES employees(id),
  FOREIGN KEY (team_id) REFERENCES teams(id)
);
CREATE TABLE team_memberships (
  id SERIAL PRIMARY KEY,
  team_id integer NOT NULL,
  employee_id integer NOT NULL,
  created_at timestamp NOT NULL DEFAULT now(),
  updated_at timestamp NOT NULL DEFAULT now(),
  FOREIGN KEY (employee_id) REFERENCES employees(id),
  FOREIGN KEY (team_id) REFERENCES teams(id)
);
CREATE INDEX index_milestones_on_epic_id ON milestones (epic_id);
CREATE INDEX index_notes_on_notable_type_and_notable_id ON notes (notable_type, notable_id);
CREATE INDEX index_positions_on_department_id ON positions (department_id);
CREATE INDEX index_positions_on_employee_id ON positions (employee_id);
CREATE INDEX index_tasks_on_employee_id ON tasks (employee_id);
CREATE INDEX index_tasks_on_team_id ON tasks (team_id);
CREATE INDEX index_tasks_on_type ON tasks (type);
CREATE INDEX index_team_memberships_on_employee_id ON team_memberships (employee_id);
CREATE INDEX index_team_memberships_on_team_id ON team_memberships (team_id);
CREATE INDEX index_teams_on_department_id ON teams (department_id);
-- migrate:down
DROP TABLE departments;
DROP TABLE employees;
DROP TABLE milestones;
DROP TABLE notes;
DROP TABLE positions;
DROP TABLE tasks;
DROP TABLE team_memberships;
DROP TABLE teams;
