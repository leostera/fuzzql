# FuzzQL — A GraphQL Fuzzy Testing Tool

## Scope

During the live-stream in [ReasonableCoding](https://twitch.tv/ostera) we
will try to hit the following milestones.

### 1. Generate a random value on a query [DONE!]

On query:

```graphql
{
  people {
    first_name
    last_name
    hometown
    alter_ego {
      name
    }
  }
}
```

We should see a random person generated:

```json
{
  "data": {
    "people": [
      {
        "first_name": "Asdf98uaf112",
        "last_name": "fnali10_@#18das",
        "hometown": ""fisodfj;aosf;esainf;ds",
        "alter_ego": {
          "name": "901fdasfsa98h"
        }
      },
    ],
  }
}
```

## Notes and Doodles

#### What would it take to read a .graphql file and generate the emulator?

`schema.graphql => native graphql api (- the transport)`

1. Reads a file

2. Parses that file into an AST:
  * ~~We'll need to write a GraphQL SDL Parser (angstrom)~~
  * If the file is the schema dump as JSON, we don't need to write a parser!

3. Generate Reason/OCaml Types that match that AST from 2
  * use OCaml Typedtree to write these types

4. Generate GraphQL Schema (from the lib) that matches the AST from 2
  * use OCaml Typedtree to write this code

4. Generate Generators for those Reason/OCaml types
  * use OCaml Typedtree to write this code
