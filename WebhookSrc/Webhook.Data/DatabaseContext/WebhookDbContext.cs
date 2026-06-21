using Microsoft.EntityFrameworkCore;
using Webhook.Data.Models;

namespace Webhook.Data;

public class WebhookDbContext : DbContext
{
    public WebhookDbContext(DbContextOptions<WebhookDbContext> options) : base(options) { }

    public DbSet<User> User { get; set; }
    public DbSet<WebhookSubscription> WebhookSubscription { get; set; }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        base.OnModelCreating(modelBuilder);

        modelBuilder.Entity<WebhookSubscription>().ToTable("User", schema: "USERS");

        modelBuilder.Entity<WebhookSubscription>().ToTable("WebhookSubscription", schema: "WEBHOOK");
    }
}
