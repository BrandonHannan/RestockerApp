using Microsoft.EntityFrameworkCore;
using Webhook.Data.Models;

namespace Webhook.Data;

public class WebhookDbContext : DbContext
{
    public WebhookDbContext(DbContextOptions<WebhookDbContext> options) : base(options) { }

    public DbSet<User> Users { get; set; }
    public DbSet<Distributor> Distributors { get; set; }
    public DbSet<Product> Products { get; set; }
    public DbSet<Stock> Stock { get; set; }
    public DbSet<StockType> StockTypes { get; set; }
    public DbSet<FufilmentChannel> fufilmentChannels { get; set; }
    public DbSet<WebhookConnection> WebhookConnections { get; set; }
    public DbSet<Alert> Alerts { get; set; }
    public DbSet<Location> Locations { get; set; }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        base.OnModelCreating(modelBuilder);

        modelBuilder.Entity<User>().ToTable("User", schema: "USERS");

        modelBuilder.Entity<User>().HasIndex(u => u.UserId).IsUnique();

        modelBuilder.Entity<Distributor>().HasIndex(d => d.DistributorID).IsUnique();

        modelBuilder.Entity<Product>().HasIndex(p => p.ProductID).IsUnique();

        modelBuilder.Entity<Product>().HasIndex(p => p.ReferenceID);

        modelBuilder.Entity<Stock>().HasIndex(s => s.StockID).IsUnique();

        modelBuilder.Entity<Stock>().HasIndex(s => s.ProductID);

        modelBuilder.Entity<StockType>().HasIndex(sT => sT.StockTypeID).IsUnique();

        modelBuilder.Entity<FufilmentChannel>().HasIndex(f => f.FufilmentChannelID).IsUnique();

        modelBuilder.Entity<WebhookConnection>().HasIndex(wC => wC.WebhookConnectionID).IsUnique();

        modelBuilder.Entity<Alert>().HasIndex(a => a.AlertID).IsUnique();

        modelBuilder.Entity<Location>().HasIndex(l => l.LocationID).IsUnique();

        modelBuilder.Entity<Location>().HasIndex(l => l.ReferenceID);

    }
}
