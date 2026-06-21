using Webhook.API.AuthHelper;
using Webhook.Data;
using Webhook.Services.UserServices;
using Webhook.Services.WebhookServices;

var builder = WebApplication.CreateBuilder(args);

// Add services to the container.

builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();


builder.Services.AddScoped<WebhookDbContext>();

builder.Services.AddScoped<UserService>();
builder.Services.AddScoped<WebhookSubscriptionService>();

builder.Services.AddTransient<JWTHelper>();

var app = builder.Build();

// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.UseSwagger();
    app.UseSwaggerUI();
}

app.UseHttpsRedirection();

app.UseMiddleware<Authenticate>();

app.UseAuthorization();

app.MapControllers();

app.Run();
