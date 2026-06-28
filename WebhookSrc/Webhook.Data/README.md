## Revert Database

```
dotnet ef database drop --force --project Webhook.Data.csproj --startup-project ../WebhookLearning/Webhook.API.csproj --context WebhookDbContext -- --Environment Development
dotnet ef migrations add InitialCreate --project Webhook.Data.csproj --startup-project ../WebhookLearning/Webhook.API.csproj --context WebhookDbContext
```

## Go Back to Initial Migration
```
dotnet ef database update 0 --startup-project ../WebhookLearning/Webhook.API.csproj --context WebhookDbContext -- --Environment Development
```

## Updating Database to the Latest Migration
```
dotnet ef database update --startup-project ../WebhookLearning/Webhook.API.csproj --context WebhookDbContext -- --Environment Development
```

## Adding Migration to the Database
```
dotnet ef migrations add InitialCreate --project Webhook.Data.csproj --startup-project ../WebhookLearning/Webhook.API.csproj --context WebhookDbContext
```