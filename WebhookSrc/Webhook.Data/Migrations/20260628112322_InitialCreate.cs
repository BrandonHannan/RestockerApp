using System;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace Webhook.Data.Migrations
{
    /// <inheritdoc />
    public partial class InitialCreate : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.EnsureSchema(
                name: "USERS");

            migrationBuilder.CreateTable(
                name: "Distributors",
                columns: table => new
                {
                    DistributorID = table.Column<Guid>(type: "uuid", nullable: false),
                    Name = table.Column<string>(type: "text", nullable: false),
                    BaseUrl = table.Column<string>(type: "text", nullable: false),
                    SitemapUrl = table.Column<string>(type: "text", nullable: false),
                    IsActive = table.Column<bool>(type: "boolean", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Distributors", x => x.DistributorID);
                });

            migrationBuilder.CreateTable(
                name: "fufilmentChannels",
                columns: table => new
                {
                    FufilmentChannelID = table.Column<Guid>(type: "uuid", nullable: false),
                    Name = table.Column<string>(type: "text", nullable: false),
                    ReferenceID = table.Column<string>(type: "text", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_fufilmentChannels", x => x.FufilmentChannelID);
                });

            migrationBuilder.CreateTable(
                name: "StockTypes",
                columns: table => new
                {
                    StockTypeID = table.Column<Guid>(type: "uuid", nullable: false),
                    Name = table.Column<string>(type: "text", nullable: false),
                    ReferenceID = table.Column<string>(type: "text", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_StockTypes", x => x.StockTypeID);
                });

            migrationBuilder.CreateTable(
                name: "User",
                schema: "USERS",
                columns: table => new
                {
                    UserId = table.Column<Guid>(type: "uuid", nullable: false),
                    Username = table.Column<string>(type: "text", nullable: false),
                    Email = table.Column<string>(type: "text", nullable: false),
                    Password = table.Column<string>(type: "text", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_User", x => x.UserId);
                });

            migrationBuilder.CreateTable(
                name: "Locations",
                columns: table => new
                {
                    LocationID = table.Column<Guid>(type: "uuid", nullable: false),
                    Name = table.Column<string>(type: "text", nullable: false),
                    ReferenceID = table.Column<string>(type: "text", nullable: false),
                    DistributorID = table.Column<Guid>(type: "uuid", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Locations", x => x.LocationID);
                    table.ForeignKey(
                        name: "FK_Locations_Distributors_DistributorID",
                        column: x => x.DistributorID,
                        principalTable: "Distributors",
                        principalColumn: "DistributorID",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateTable(
                name: "Products",
                columns: table => new
                {
                    ProductID = table.Column<Guid>(type: "uuid", nullable: false),
                    Name = table.Column<string>(type: "text", nullable: false),
                    Description = table.Column<string>(type: "text", nullable: false),
                    Price = table.Column<double>(type: "double precision", nullable: false),
                    ReferenceID = table.Column<string>(type: "text", nullable: false),
                    ProductUrl = table.Column<string>(type: "text", nullable: false),
                    ProductImgUrl = table.Column<string>(type: "text", nullable: false),
                    DistributorID = table.Column<Guid>(type: "uuid", nullable: false),
                    IsPreOrder = table.Column<bool>(type: "boolean", nullable: false),
                    PreOrderDate = table.Column<string>(type: "text", nullable: false),
                    IsAvailable = table.Column<bool>(type: "boolean", nullable: false),
                    FufilmentChannelID = table.Column<Guid>(type: "uuid", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Products", x => x.ProductID);
                    table.ForeignKey(
                        name: "FK_Products_Distributors_DistributorID",
                        column: x => x.DistributorID,
                        principalTable: "Distributors",
                        principalColumn: "DistributorID",
                        onDelete: ReferentialAction.Cascade);
                    table.ForeignKey(
                        name: "FK_Products_fufilmentChannels_FufilmentChannelID",
                        column: x => x.FufilmentChannelID,
                        principalTable: "fufilmentChannels",
                        principalColumn: "FufilmentChannelID",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateTable(
                name: "WebhookConnections",
                columns: table => new
                {
                    WebhookConnectionID = table.Column<Guid>(type: "uuid", nullable: false),
                    Url = table.Column<string>(type: "text", nullable: false),
                    DistributorID = table.Column<Guid>(type: "uuid", nullable: false),
                    StockTypeID = table.Column<Guid>(type: "uuid", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_WebhookConnections", x => x.WebhookConnectionID);
                    table.ForeignKey(
                        name: "FK_WebhookConnections_Distributors_DistributorID",
                        column: x => x.DistributorID,
                        principalTable: "Distributors",
                        principalColumn: "DistributorID",
                        onDelete: ReferentialAction.Cascade);
                    table.ForeignKey(
                        name: "FK_WebhookConnections_StockTypes_StockTypeID",
                        column: x => x.StockTypeID,
                        principalTable: "StockTypes",
                        principalColumn: "StockTypeID",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateTable(
                name: "Stock",
                columns: table => new
                {
                    StockID = table.Column<Guid>(type: "uuid", nullable: false),
                    StockAvailable = table.Column<int>(type: "integer", nullable: false),
                    StockTypeID = table.Column<Guid>(type: "uuid", nullable: false),
                    ProductID = table.Column<Guid>(type: "uuid", nullable: false),
                    LocationID = table.Column<Guid>(type: "uuid", nullable: true),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    Updated = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    IsDeleted = table.Column<bool>(type: "boolean", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Stock", x => x.StockID);
                    table.ForeignKey(
                        name: "FK_Stock_Products_ProductID",
                        column: x => x.ProductID,
                        principalTable: "Products",
                        principalColumn: "ProductID",
                        onDelete: ReferentialAction.Cascade);
                    table.ForeignKey(
                        name: "FK_Stock_StockTypes_StockTypeID",
                        column: x => x.StockTypeID,
                        principalTable: "StockTypes",
                        principalColumn: "StockTypeID",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateTable(
                name: "Alerts",
                columns: table => new
                {
                    AlertID = table.Column<Guid>(type: "uuid", nullable: false),
                    StockID = table.Column<Guid>(type: "uuid", nullable: false),
                    StockChange = table.Column<int>(type: "integer", nullable: false),
                    Created = table.Column<DateTime>(type: "timestamp with time zone", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Alerts", x => x.AlertID);
                    table.ForeignKey(
                        name: "FK_Alerts_Stock_StockID",
                        column: x => x.StockID,
                        principalTable: "Stock",
                        principalColumn: "StockID",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateIndex(
                name: "IX_Alerts_AlertID",
                table: "Alerts",
                column: "AlertID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_Alerts_StockID",
                table: "Alerts",
                column: "StockID");

            migrationBuilder.CreateIndex(
                name: "IX_Distributors_DistributorID",
                table: "Distributors",
                column: "DistributorID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_fufilmentChannels_FufilmentChannelID",
                table: "fufilmentChannels",
                column: "FufilmentChannelID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_Locations_DistributorID",
                table: "Locations",
                column: "DistributorID");

            migrationBuilder.CreateIndex(
                name: "IX_Locations_LocationID",
                table: "Locations",
                column: "LocationID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_Locations_ReferenceID",
                table: "Locations",
                column: "ReferenceID");

            migrationBuilder.CreateIndex(
                name: "IX_Products_DistributorID",
                table: "Products",
                column: "DistributorID");

            migrationBuilder.CreateIndex(
                name: "IX_Products_FufilmentChannelID",
                table: "Products",
                column: "FufilmentChannelID");

            migrationBuilder.CreateIndex(
                name: "IX_Products_ProductID",
                table: "Products",
                column: "ProductID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_Products_ReferenceID",
                table: "Products",
                column: "ReferenceID");

            migrationBuilder.CreateIndex(
                name: "IX_Stock_ProductID",
                table: "Stock",
                column: "ProductID");

            migrationBuilder.CreateIndex(
                name: "IX_Stock_StockID",
                table: "Stock",
                column: "StockID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_Stock_StockTypeID",
                table: "Stock",
                column: "StockTypeID");

            migrationBuilder.CreateIndex(
                name: "IX_StockTypes_StockTypeID",
                table: "StockTypes",
                column: "StockTypeID",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_User_UserId",
                schema: "USERS",
                table: "User",
                column: "UserId",
                unique: true);

            migrationBuilder.CreateIndex(
                name: "IX_WebhookConnections_DistributorID",
                table: "WebhookConnections",
                column: "DistributorID");

            migrationBuilder.CreateIndex(
                name: "IX_WebhookConnections_StockTypeID",
                table: "WebhookConnections",
                column: "StockTypeID");

            migrationBuilder.CreateIndex(
                name: "IX_WebhookConnections_WebhookConnectionID",
                table: "WebhookConnections",
                column: "WebhookConnectionID",
                unique: true);
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "Alerts");

            migrationBuilder.DropTable(
                name: "Locations");

            migrationBuilder.DropTable(
                name: "User",
                schema: "USERS");

            migrationBuilder.DropTable(
                name: "WebhookConnections");

            migrationBuilder.DropTable(
                name: "Stock");

            migrationBuilder.DropTable(
                name: "Products");

            migrationBuilder.DropTable(
                name: "StockTypes");

            migrationBuilder.DropTable(
                name: "Distributors");

            migrationBuilder.DropTable(
                name: "fufilmentChannels");
        }
    }
}
